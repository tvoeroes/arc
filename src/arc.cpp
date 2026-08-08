#include "arc/arc.hpp"

#include "arc/detail/name_store.hpp"
#include "arc/detail/scheduler.hpp"
#include "arc/util/algorithms.hpp"
#include "arc/util/check.hpp"
#include "arc/util/guard.hpp"
#include "arc/util/on_scope_exit.hpp"

#include <atomic>
#include <cstdio>
#include <print>

#define arc_SCHEDULER_TRACE_WORKER_LIFETIME 0

#if arc_TRACE_INSTRUMENTATION_ENABLE
namespace
{
	auto & name_map_instance()
	{
		struct name_map
		{
			arc::util::shared_guard<std::unordered_map<const void *, arc::detail::zone_info>> names;
			~name_map() { arc_CHECK_Require(names.read_only()->size() == 0); }
		};
		static name_map map;
		return map;
	}
}

arc::detail::zone_info arc::detail::get_zone_info(
	const void * address, arc::detail::zone_info fallback)
{
	auto names_it = name_map_instance().names.read_only();
	if (auto it = names_it->find(address); it != names_it->end())
		return it->second;
	else
		return fallback;
}

void arc::detail::set_zone_info(const void * address, arc::detail::zone_info info)
{
	auto names_it = name_map_instance().names.read_and_write();
	names_it->insert_or_assign(address, info);
}

void arc::detail::clear_zone_info(const void * address)
{
	auto names_it = name_map_instance().names.read_and_write();
	names_it->erase(address);
}

void arc_TRACE_REPORT_SHARED_CLOSURE_TYPE()
{
	static std::atomic_flag reported;
	if (reported.test_and_set(std::memory_order_relaxed))
		return;

	arc_TRACE_MESSAGE(true, "compiler bug detected - workaround with degraded performance");

	std::fflush(stdout);
	std::println(
		stderr,
		"/---- compiler bug detected - workaround with degraded performance -----\n"
		"| `template <auto = []{{}}>` does not give every call site a distinct closure type\n"
		"| on this compiler, so named trace zones fall back to transient ones, which have\n"
		"| the profiler allocate a source location per zone. Zone names stay correct.\n"
		"\\-----------------------------------------------------------------------");
	std::fflush(stderr);
}
#endif

const char * arc::detail::leak_new_c_string(std::string_view string)
{
	const size_t string_size = string.size();
	char * c_string = new char[string_size + 1];
	std::copy_n(string.data(), string_size, c_string);
	c_string[string_size] = '\0';
	return c_string;
}

namespace
{
	template <typename T, typename Q, typename C, typename M>
	void ThreadSafePush(T && element, Q & queue, C & conditionVariable, M & mutex)
	{
		arc::util::on_scope_exit _ = [&conditionVariable] { conditionVariable.notify_one(); };
		std::lock_guard lk{ mutex };
		queue.emplace(std::move(element));
	}

	template <typename T, typename V, typename C, typename M>
	void ThreadSafeInsertSorted(T && element, V & vector, C & conditionVariable, M & mutex)
	{
		arc::util::on_scope_exit _ = [&conditionVariable] { conditionVariable.notify_one(); };
		std::lock_guard lk{ mutex };
		arc::util::insert_sorted(
			vector, std::move(element),
			[](const auto & lhs, const auto & rhs) { return lhs.first < rhs.first; });
	}

	template <typename G, typename T, typename C, typename M, typename V>
	std::optional<arc::detail::scheduler::task> ThreadSafeWorkPop(
		G & highPrioTasks, T & tasks, V & timedTasks, C & conditionVariable, M & mutex,
		const std::stop_token & stopToken)
	{
		arc_TRACE_EVENT_SCOPED(arc_TRACE_WORKER_IDLE);

		std::unique_lock lk{ mutex };

		bool timerReady = false;
		bool haveValue = false;
		bool stopRequested = false;
		bool haveHighPrioTasks = false;

		auto waitPredicate = [&highPrioTasks, &timedTasks, &tasks, &timerReady, &haveValue,
							  &stopRequested, &haveHighPrioTasks, &stopToken] {
			haveHighPrioTasks = highPrioTasks.size();
			timerReady = timedTasks.size() && timedTasks[0].first <= arc::clock::now();
			bool haveWorkScheduled = tasks.size();
			stopRequested = !timedTasks.size() && stopToken.stop_requested();
			haveValue = haveHighPrioTasks || timerReady || haveWorkScheduled || stopRequested;
			return haveValue;
		};

		while (!haveValue)
		{
			if (timedTasks.size())
			{
				arc::time_point until = timedTasks[0].first;
				conditionVariable.wait_until(lk, stopToken, until, waitPredicate);
			}
			else
			{
				conditionVariable.wait(lk, stopToken, waitPredicate);
			}
		}

		if (haveHighPrioTasks)
		{
			return arc::util::queue_pop(highPrioTasks);
		}
		else if (timerReady)
		{
			arc_CHECK_Assert(!!timedTasks.size());
			arc::detail::scheduler::task handle = std::move(timedTasks.front().second);
			timedTasks.erase(timedTasks.begin());
			return handle;
		}
		else if (tasks.size())
		{
			if constexpr (arc::detail::scheduler::ArcSchedulerWorkPool_USING_QUEUE)
				return arc::util::queue_pop(tasks);
			else
				return arc::util::stack_pop(tasks);
		}
		else if (stopRequested)
		{
			return std::nullopt;
		}

		arc_CHECK_Require(false);
	}
}

arc::context::~context()
{
	store.set_empty_once_callback([this] { this->scheduler.request_stop(); });
}

arc::detail::scheduler::scheduler(std::thread::id mainThreadId, size_t workerThreadCount)
	: mainThreadId{ mainThreadId }
{
	arc_TRACE_CONTAINER_CONFIGURE(
		workerThreadWork.highPrioTasks, "workerThreadWork.highPrioTasks.size()");
	arc_TRACE_CONTAINER_CONFIGURE(workerThreadWork.tasks, "workerThreadWork.tasks.size()");

	arc_TRACE_CONTAINER_CONFIGURE(
		mainThreadWork.highPrioTasks, "mainThreadWork.highPrioTasks.size()");
	arc_TRACE_CONTAINER_CONFIGURE(mainThreadWork.tasks, "mainThreadWork.tasks.size()");

	start_workers(workerThreadCount);
}

void arc::detail::scheduler::schedule(
	task && task, const std::optional<arc::time_point> & timePoint, bool mainThread)
{
	arc_CHECK_Require(task.function);

	work_pool & work = mainThread ? mainThreadWork : workerThreadWork;

	if (timePoint)
		ThreadSafeInsertSorted(
			work_pool::timed_task{ *timePoint, std::move(task) }, work.timers, work.cv, work.mtx);
	else
		ThreadSafePush(task, work.tasks, work.cv, work.mtx);
}

void arc::detail::scheduler::schedule(task && task, bool mainThread, bool highPrio)
{
	arc_CHECK_Precondition(task.function);
	work_pool & work = mainThread ? mainThreadWork : workerThreadWork;
	if (highPrio)
		ThreadSafePush(std::move(task), work.highPrioTasks, work.cv, work.mtx);
	else
		ThreadSafePush(std::move(task), work.tasks, work.cv, work.mtx);
}

void arc::detail::scheduler::worker(std::stop_token stopToken, std::optional<size_t> workerIndex)
{
#if arc_SCHEDULER_TRACE_WORKER_LIFETIME
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);
#endif

#if arc_TRACE_INSTRUMENTATION_ENABLE
	if (workerIndex)
	{
		std::string name = "ArcWorker " + std::to_string(*workerIndex);
		tracy::SetThreadName(name.c_str());
	}
#endif

	bool mainThread = mainThreadId == std::this_thread::get_id();
	work_pool & work = mainThread ? mainThreadWork : workerThreadWork;

	while (true)
	{
		std::optional<arc::detail::scheduler::task> task = ThreadSafeWorkPop(
			work.highPrioTasks, work.tasks, work.timers, work.cv, work.mtx, stopToken);

		if (task)
		{
			arc_CHECK_Assert(task->function);
			if (task->zone)
			{
				arc_TRACE_ZONE_SCOPE scope{ task->zone, arc_TRACE_CORO };
				task->function();
			}
			else
			{
				task->function();
			}
		}
		else
		{
			break;
		}
	}
}

void arc::detail::scheduler::start_workers(size_t count)
{
	arc_CHECK_Assert(!workers.size());
	workers.reserve(count);
	for (size_t i = 0; i < count; i++)
		workers.emplace_back(&arc::detail::scheduler::worker, this, stopSource.get_token(), i);
}

void arc::detail::scheduler::assist(std::stop_token && stopToken)
{
	worker(std::move(stopToken), std::nullopt);
}

void arc::detail::scheduler::assist() { worker(stopSource.get_token(), std::nullopt); }

void arc::detail::scheduler::request_stop() { stopSource.request_stop(); }

arc::detail::scheduler::~scheduler()
{
	assist();
	for (std::thread & worker : workers)
		worker.join();

	arc_CHECK_Require(mainThreadWork.timers.size() == 0);
	arc_CHECK_Require(mainThreadWork.highPrioTasks.size() == 0);
	arc_CHECK_Require(mainThreadWork.tasks.size() == 0);

	arc_CHECK_Require(workerThreadWork.timers.size() == 0);
	arc_CHECK_Require(workerThreadWork.highPrioTasks.size() == 0);
	arc_CHECK_Require(workerThreadWork.tasks.size() == 0);
}

void arc::detail::store::release_reference(arc::detail::handle && coroHandle)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

	arc_CHECK_Precondition(coroHandle && coroHandle.storeEntry);
	arc::detail::store_entry & storeEntry = *coroHandle.storeEntry;
	arc::detail::control_block & controlBlock = coroHandle->second;
	const arc::detail::key & theKey = coroHandle->first;

	{
		auto waiters = controlBlock.waiters.read_and_write();

		auto oldRefCount = controlBlock.referenceCount.fetch_sub(1, std::memory_order::acq_rel);
		coroHandle.abandon();
		arc_CHECK_Assert(oldRefCount > 0);
		if (oldRefCount > 1)
		{
			return;
		}

		if (!*waiters)
		{
			waiters->emplace();
		}
	}

	controlBlock.result.reset();

	auto dataHandle = data.read_and_write();

	{
		auto waiters = controlBlock.waiters.read_and_write();
		arc_CHECK_Assert(*waiters);

		auto refCount = controlBlock.referenceCount.load(std::memory_order::acquire);
		if (refCount > 0)
		{
			theKey.call(storeEntry);
			return;
		}
		else
		{
			arc_CHECK_Assert(!(*waiters)->continuations.size());
		}
	}

#if 0
	auto oldRefCount = controlBlock.referenceCount.fetch_sub(1, std::memory_order::acq_rel);
	arc_CHECK_Assert(oldRefCount > 0);
	if (oldRefCount > 1)
		return;
#endif

	auto it = dataHandle->store.find(theKey);
	arc_CHECK_Assert(it != dataHandle->store.end());

	{
		arc_CHECK_Precondition(controlBlock.referenceCount.load(std::memory_order::relaxed) == 0);
		dataHandle->store.erase(it);
	}

	if (!dataHandle->store.size())
	{
		while (dataHandle->emptyOnceCallbacks.size())
		{
			dataHandle->emptyOnceCallbacks.front()();
			dataHandle->emptyOnceCallbacks.pop();
		}
	}
}

void arc::detail::store::set_empty_once_callback(arc::function<void()> && emptyOnceCallback)
{
	if (!emptyOnceCallback)
		return;

	auto dataHandle = data.read_and_write();

	if (!dataHandle->store.size())
	{
		arc_CHECK_Assert(!dataHandle->emptyOnceCallbacks.size());
		emptyOnceCallback();
	}
	else
	{
		dataHandle->emptyOnceCallbacks.push(std::move(emptyOnceCallback));
	}
}

arc::detail::handle::handle(arc::detail::store_entry * storeEntry)
	: storeEntry{ storeEntry }
{
	acquire();
}

arc::detail::handle::handle(const handle & other)
	: storeEntry{ other.storeEntry }
{
	acquire();
}

arc::detail::handle & arc::detail::handle::operator=(const handle & other)
{
	if (this != std::addressof(other))
	{
		release();
		storeEntry = other.storeEntry;
		acquire();
	}

	return *this;
}

arc::detail::handle::handle(handle && other)
	: storeEntry{ std::exchange(other.storeEntry, nullptr) }
{}

arc::detail::handle & arc::detail::handle::operator=(handle && other)
{
	if (this != std::addressof(other))
	{
		release();
		storeEntry = std::exchange(other.storeEntry, nullptr);
	}

	return *this;
}

arc::detail::handle::~handle() { release(); }

arc::detail::handle & arc::detail::handle::operator=(std::nullptr_t)
{
	release();

	return *this;
}

void arc::detail::handle::release()
{
	if (storeEntry)
		storeEntry->second.remove_reference(std::move(*this));
}

void arc::detail::handle::acquire()
{
	if (storeEntry)
		storeEntry->second.add_reference();
}

void arc::detail::handle::abandon() { storeEntry = nullptr; }

arc::detail::handle::operator bool() const noexcept { return storeEntry; }

arc::detail::store_entry * arc::detail::handle::operator->() const noexcept
{
	arc_CHECK_Precondition(storeEntry);
	return storeEntry;
}

void arc::detail::control_block::add_reference() noexcept
{
	referenceCount.fetch_add(1, std::memory_order::relaxed);
}

void arc::detail::control_block::remove_reference(arc::detail::handle && coroHandle)
{
	arc_CHECK_Assert(coroHandle);

	auto refCount = referenceCount.load(std::memory_order::acquire);

	while (refCount != 1)
	{
		arc_CHECK_Assert(refCount > 0);

		if (referenceCount.compare_exchange_weak(
				refCount, refCount - 1, std::memory_order::acq_rel))
		{
			coroHandle.abandon();
			return;
		}
	}

	arc::context & ctx = coroHandle->first.get_ctx();

	struct NonCopyableHandle
	{
		NonCopyableHandle(arc::detail::handle && handle)
			: handle{ std::move(handle) }
		{}
		arc::detail::handle handle;

		NonCopyableHandle(const NonCopyableHandle &) = delete;
		NonCopyableHandle & operator=(const NonCopyableHandle &) = delete;
		NonCopyableHandle(NonCopyableHandle &&) = default;
		NonCopyableHandle & operator=(NonCopyableHandle &&) = default;
	};

	static_assert(sizeof(NonCopyableHandle) == sizeof(void *));

	ctx.scheduler.schedule(
		{
#if arc_FUNCTION_IS_MOVE_ONLY
			[handle = NonCopyableHandle{ std::move(coroHandle) }]() mutable {
				arc::context & ctx = handle.handle->first.get_ctx();
				ctx.store.release_reference(std::move(handle.handle));
			},
#else
			[handle = std::make_shared<arc::detail::handle>(std::move(coroHandle))]() {
				arc::context & ctx = (*handle)->first.get_ctx();
				ctx.store.release_reference(std::move(*handle));
			},
#endif
			"release_reference",
		},
		false, true);
}

bool arc::detail::control_block::try_add_continuation(
	arc::function<void()> && continuation, arc::detail::zone_info zone)
{
	arc_CHECK_Precondition(continuation);

	auto comp = waiters.read_and_write();
	if (!comp->has_value())
		return false;
	(*comp)->continuations.emplace_back(
		arc::detail::control_block::Waiters::Continuation{ std::move(continuation), zone });
	return true;
}

arc::detail::control_block::~control_block()
{
	arc_CHECK_Precondition(referenceCount.load(std::memory_order::relaxed) == 0);
	arc_CHECK_Precondition(result.holds_nothing());
}

void arc::detail::coro_promise_base::unhandled_exception() noexcept
{
	if (published_early_)
		std::terminate(); /** Unsupported situation. Maybe the correct behavior would be to just
							 return here instead of terminating and hence not reporting exceptions
							 that happen after publishing the result. */
	if (!self_handle_->second.result.holds_nothing())
		std::terminate(); /** Unsupported situation. Probably unhandled_exception() after
							 arc::promise_proxy::construct(). */

	self_handle_->second.result.set_unhandled_exception(std::current_exception());
	publish_result();
}

void arc::detail::coro_promise_base::publish_result() noexcept /** Not exception-safe therefore
																  noexcept */
{
	auto comp = self_handle_->second.waiters.read_and_write();

	arc_CHECK_Precondition(comp->has_value());

	for (arc::detail::control_block::Waiters::Continuation & continuation : (*comp)->continuations)
		self_handle_->first.get_ctx().schedule_on_worker_thread(
			std::move(continuation.function), continuation.zone);

	comp->reset();
}

arc::detail::coro_promise_base::~coro_promise_base()
{
	arc_CHECK_Precondition(!self_handle_->second.waiters.read_only()->has_value());
}

arc::context::context()
	: context{ arc::options{} }
{}

arc::context::context(const arc::options & options)
	: options_{ options }
	, scheduler{ options.mainThreadId, options.workerThreadCount }
{}

const arc::options & arc::context::options() const { return options_; }

void arc::context::schedule_on_worker_thread(std::coroutine_handle<> handle)
{
	return scheduler.schedule(
		detail::scheduler::task{ handle, arc::detail::get_zone_info(handle.address()) },
		std::nullopt, false);
}

void arc::context::schedule_on_worker_thread_after(
	std::coroutine_handle<> handle, arc::time_point timePoint)
{
	return scheduler.schedule(
		detail::scheduler::task{ handle, arc::detail::get_zone_info(handle.address()) }, timePoint,
		false);
}

void arc::context::schedule_on_worker_thread_after(
	arc::function<void()> && task, arc::time_point timePoint, arc::detail::zone_info zone)
{
	return scheduler.schedule(detail::scheduler::task{ std::move(task), zone }, timePoint, false);
}

void arc::context::schedule_on_worker_thread(
	arc::function<void()> && task, arc::detail::zone_info zone)
{
	return scheduler.schedule({ std::move(task), zone }, false, false);
}

void arc::context::schedule_on_main_thread(std::coroutine_handle<> handle)
{
	return scheduler.schedule(
		detail::scheduler::task{ handle, arc::detail::get_zone_info(handle.address()) },
		std::nullopt, true);
}

void arc::context::schedule_on_main_thread_after(
	std::coroutine_handle<> handle, arc::time_point timePoint)
{
	return scheduler.schedule(
		detail::scheduler::task{ handle, arc::detail::get_zone_info(handle.address()) }, timePoint,
		true);
}

void arc::context::schedule_on_main_thread_after(
	arc::function<void()> && task, arc::time_point timePoint, arc::detail::zone_info zone)
{
	return scheduler.schedule(detail::scheduler::task{ std::move(task), zone }, timePoint, true);
}

void arc::context::schedule_on_main_thread(
	arc::function<void()> && task, arc::detail::zone_info zone)
{
	return scheduler.schedule({ std::move(task), zone }, true, false);
}

template <std::integral T>
static std::optional<T> from_string(std::string_view str)
{
	if constexpr (std::is_same_v<T, bool>)
	{
		if (str == "true")
			return true;
		else if (str == "false")
			return false;
		else
			return std::nullopt;
	}
	else
	{
		T result = 0;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
		if (ptr != (str.data() + str.size()) || ec != std::errc{})
			return std::nullopt;
		else
			return result;
	}
}

template <typename T>
static T getArg(std::string_view arg, std::span<const char *> args, const T & fallback)
{
	auto it = std::ranges::find_last(args.begin(), args.end(), arg).begin();

	if (it != args.end() && ++it != args.end())
	{
		return from_string<T>(*it).value_or(fallback);
	}
	else
	{
		return fallback;
	}
}

static std::span<const char * const> make_args(int argc, char * argv[])
{
	if (argc > 0)
		return { const_cast<const char **>(argv), size_t(argc) };
	else
		return {};
}

static std::vector<const char *> make_args(
	std::span<const char * const> baseArgs, int argc, char * argv[])
{
	std::vector<const char *> args;

	std::span<const char * const> cmdArgs = make_args(argc, argv);

	args.reserve(cmdArgs.size() + baseArgs.size());

	auto cmdArgsIt = cmdArgs.begin();
	if (cmdArgsIt != cmdArgs.end())
		args.emplace_back(*cmdArgsIt++);

	for (const char * a : baseArgs)
		args.emplace_back(a);

	while (cmdArgsIt != cmdArgs.end())
		args.emplace_back(*cmdArgsIt++);

	return args;
}

arc::options arc::options::from_args(
	std::span<const char * const> baseArgs, int argc, char * argv[])
{
	std::vector<const char *> args = make_args(baseArgs, argc, argv);

	const bool withMainThread = getArg<bool>("--withMainThread", args, false);
	size_t workerThreadCount = getArg(
		"--workerThreadCount", args,
		size_t(std::max<unsigned int>(std::thread::hardware_concurrency(), 2)) - 1);
	return {
		.workerThreadCount = workerThreadCount,
		.mainThreadId = withMainThread ? std::this_thread::get_id() : std::thread::id{},
		.args = std::move(args),
	};
}

arc::detail::globals::~globals()
{
	auto globalsIt = store.read_and_write();
	while (globalsIt->size())
		globalsIt->pop();
}

void arc::detail::globals::add(arc::detail::handle && global)
{
	store.read_and_write()->push(std::move(global));
}

arc::detail::store::store()
{
	arc_TRACE_CONTAINER_CONFIGURE(data.read_and_write()->store, "arc::detail::store");
}

arc::detail::store::~store() { arc_CHECK_Precondition(!data.read_and_write()->store.size()); }

arc::detail::handle arc::detail::store::retrieve_reference(
	arc::detail::key && key
#if arc_WITH_SOURCE_LOCATION
	,
	const std::source_location & sourceLocation
#endif
)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

	auto dataHandle = data.read_and_write();

	auto insertion = dataHandle->store.try_emplace(std::move(key));
	auto it = insertion.first;

	if (insertion.second)
	{
		it->first.call(*it);
	}

#if arc_TRACE_INSTRUMENTATION_ENABLE && arc_WITH_SOURCE_LOCATION
	it->second.requestLocations.emplace_back(sourceLocation);
#endif

	return arc::detail::handle{ &*it };
}

#if arc_TRACE_INSTRUMENTATION_ENABLE && 0
/** NOTE: there are more new and delete operators that should be replaced */

void * operator new(std::size_t size)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_MEMORY);

	if (size == 0)
		size = 1;

	if (void * ptr = std::malloc(size))
	{
		TracyAlloc(ptr, size);
		return ptr;
	}

	throw std::bad_alloc();
}

void * operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void * ptr) noexcept
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_MEMORY);

	TracyFree(ptr);
	std::free(ptr);
}

void operator delete[](void * ptr) noexcept { ::operator delete(ptr); }
void operator delete(void * ptr, std::size_t sz) noexcept { ::operator delete(ptr); }
void operator delete[](void * ptr, std::size_t sz) noexcept { ::operator delete(ptr); }

#endif
