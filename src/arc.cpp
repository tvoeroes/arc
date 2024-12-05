#include "arc/arc.hpp"

#include "arc/extra/algorithms.hpp"
#include "arc/extra/on_scope_exit.hpp"
#include "arc/util/check.hpp"
#include "arc/util/debug.hpp"

namespace
{
	template <typename T, typename Q, typename C, typename M>
	void ThreadSafePush(T && element, Q & queue, C & conditionVariable, M & mutex)
	{
		arc::extra::on_scope_exit notifyOne{ [&conditionVariable]
											 { conditionVariable.notify_one(); } };
		std::lock_guard lk{ mutex };
		queue.emplace(std::move(element));
	}

	template <typename T, typename Q1, typename Q2, typename C, typename M>
	void ThreadSafePush(
		T && element, Q1 & referenceQueue, Q2 & dataQueue, C & conditionVariable, M & mutex)
	{
		arc::extra::on_scope_exit notifyOne{ [&conditionVariable]
											 { conditionVariable.notify_one(); } };
		std::lock_guard lk{ mutex };
		referenceQueue.emplace(nullptr);
		dataQueue.emplace(std::move(element));
	}

	template <typename T, typename V, typename C, typename M>
	void ThreadSafeInsertSorted(T && element, V & vector, C & conditionVariable, M & mutex)
	{
		arc::extra::on_scope_exit notifyOne{ [&conditionVariable]
											 { conditionVariable.notify_one(); } };
		std::lock_guard lk{ mutex };
		arc::extra::insert_sorted(vector, std::move(element));
	}

	template <typename Q, typename G, typename T>
	struct WorkPopRes
	{
		typename Q::value_type coroutine;
		typename G::value_type handle;
		typename T::value_type task;
	};

	template <typename Q, typename G, typename T, typename C, typename M, typename S, typename V>
	std::optional<WorkPopRes<Q, G, T>> ThreadSafeWorkPop(
		Q & queue, G & garbage, const size_t & unusedCachesize, T & tasks, V & vector,
		C & conditionVariable, M & mutex, S & stopToken)
	{
		std::unique_lock lk{ mutex };

		bool timerReady = false;
		bool haveValue = false;
		bool stopRequested = false;
		bool haveGarbage = false;

		auto waitPredicate = [&queue, &garbage, &unusedCachesize, &vector, &tasks, &timerReady,
							  &haveValue, &stopRequested, &haveGarbage, &stopToken]
		{
			haveGarbage = garbage.size() > unusedCachesize;
			timerReady = vector.size() && vector[0].first <= arc::clock::now();
			bool haveWorkScheduled = queue.size();
			stopRequested = !vector.size() && stopToken.stop_requested();
			haveValue = haveGarbage || timerReady || haveWorkScheduled || stopRequested;
			return haveValue;
		};

		while (!haveValue)
		{
			if (vector.size())
			{
				arc::time_point until = vector[0].first;
				conditionVariable.wait_until(lk, stopToken, until, waitPredicate);
			}
			else
			{
				conditionVariable.wait(lk, stopToken, waitPredicate);
			}
		}

		if (haveGarbage)
		{
			return WorkPopRes<Q, G, T>{ nullptr, arc::extra::queue_pop(garbage) };
		}
		else if (vector.size() || queue.size())
		{
			arc_CHECK_Assert(timerReady ? !!vector.size() : !!queue.size());

			std::coroutine_handle handle = [&queue, &vector, &timerReady]
			{
				arc::extra::on_scope_exit pop{
					[&queue, &vector, &timerReady]
					{
						if (timerReady)
							vector.erase(vector.begin());
						else
							queue.pop();
					},
				};

				return timerReady ? std::move(vector).front().second : std::move(queue).front();
			}();

			return WorkPopRes<Q, G, T>{ handle,
										{},
										handle ? nullptr : arc::extra::queue_pop(tasks) };
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

arc::detail::scheduler::scheduler(
	arc::detail::name_store & names, std::thread::id mainThreadId, size_t workerThreadCount)
	: names{ names }
	, mainThreadId{ mainThreadId }
{
	arc_TRACE_CONTAINER_CONFIGURE(workerThreadWork.work, "workerThreadWork.work.size()");
	arc_TRACE_CONTAINER_CONFIGURE(
		workerThreadWork.potentiallyUnused, "workerThreadWork.potentiallyUnused.size()");
	arc_TRACE_CONTAINER_CONFIGURE(workerThreadWork.tasks, "workerThreadWork.tasks.size()");

	arc_TRACE_CONTAINER_CONFIGURE(mainThreadWork.work, "mainThreadWork.work.size()");
	arc_TRACE_CONTAINER_CONFIGURE(
		mainThreadWork.potentiallyUnused, "mainThreadWork.potentiallyUnused.size()");
	arc_TRACE_CONTAINER_CONFIGURE(mainThreadWork.tasks, "mainThreadWork.tasks.size()");

	start_workers(workerThreadCount);
}

void arc::detail::scheduler::schedule(
	std::coroutine_handle<> awaiter, const std::optional<arc::time_point> & timePoint,
	bool mainThread)
{
	arc_CHECK_Require(awaiter);

	ArcSchedulerWorkPool & work = mainThread ? mainThreadWork : workerThreadWork;

	if (timePoint)
		ThreadSafeInsertSorted(std::pair{ *timePoint, awaiter }, work.timers, work.cv, work.mtx);
	else
		ThreadSafePush(awaiter, work.work, work.cv, work.mtx);
}

void arc::detail::scheduler::schedule(std::move_only_function<void()> && task, bool mainThread)
{
	arc_CHECK_Precondition(task);
	ArcSchedulerWorkPool & work = mainThread ? mainThreadWork : workerThreadWork;
	ThreadSafePush(std::move(task), work.work, work.tasks, work.cv, work.mtx);
}

void arc::detail::scheduler::worker(std::stop_token stopToken, std::optional<size_t> workerIndex)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

#if arc_TRACE_INSTRUMENTATION_ENABLE
	if (workerIndex)
	{
		std::string name = "ArcWorker " + std::to_string(*workerIndex);
		tracy::SetThreadName(name.c_str());
	}
#endif

	bool mainThread = mainThreadId == std::this_thread::get_id();
	ArcSchedulerWorkPool & work = mainThread ? mainThreadWork : workerThreadWork;

	while (std::optional task = ThreadSafeWorkPop(
			   work.work, work.potentiallyUnused, work.unusedCachesize, work.tasks, work.timers,
			   work.cv, work.mtx, stopToken))
	{
		arc_CHECK_Assert(
			int32_t(!!task->coroutine) + int32_t(!!task->handle) + int32_t(!!task->task) == 1);

		if (task->coroutine)
		{
			arc_TRACE_EVENT_SCOPED_T(arc_TRACE_CORO, names.get_name(task->coroutine));
			task->coroutine.resume();
		}
		else if (task->task)
		{
			arc_TRACE_EVENT_SCOPED_N(arc_TRACE_CORO, "Function");
			task->task();
		}
		else
		{
			arc_CHECK_Assert(task->handle);
			task->handle->second.ctx.store.release_reference(std::move(task->handle));
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

	arc_CHECK_Require(mainThreadWork.work.size() == 0);
	arc_CHECK_Require(mainThreadWork.timers.size() == 0);
	arc_CHECK_Require(mainThreadWork.potentiallyUnused.size() == 0);
	arc_CHECK_Require(mainThreadWork.tasks.size() == 0);

	arc_CHECK_Require(workerThreadWork.work.size() == 0);
	arc_CHECK_Require(workerThreadWork.timers.size() == 0);
	arc_CHECK_Require(workerThreadWork.potentiallyUnused.size() == 0);
	arc_CHECK_Require(workerThreadWork.tasks.size() == 0);
}

void arc::detail::scheduler::unused(arc::detail::handle && handle)
{
	bool mainThread = false;
	ArcSchedulerWorkPool & work = mainThread ? mainThreadWork : workerThreadWork;
	ThreadSafePush(std::move(handle), work.potentiallyUnused, work.cv, work.mtx);
}

void arc::detail::store::release_reference(arc::detail::handle && coroHandle)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

	arc_CHECK_Precondition(coroHandle && coroHandle.storeEntry);
	arc::detail::store_entry & storeEntry = *coroHandle.storeEntry;
	arc::detail::control_block & controlBlock = coroHandle->second;
	const arc::detail::key & theKey = coroHandle->first;

	{
		auto waiters = controlBlock.waiters.ReadAndWrite();

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

	if (controlBlock.exception)
	{
		arc_CHECK_Assert(!controlBlock.value && !controlBlock.deleter);
		controlBlock.exception = nullptr;
	}
	else
	{
		arc_CHECK_Assert(controlBlock.value && controlBlock.deleter);
		controlBlock.deleter(controlBlock.value);
		controlBlock.deleter = nullptr;
		controlBlock.value = nullptr;
	}

	auto dataHandle = data.ReadAndWrite();

	{
		auto waiters = controlBlock.waiters.ReadAndWrite();
		arc_CHECK_Assert(*waiters);

		auto refCount = controlBlock.referenceCount.load(std::memory_order::acquire);
		if (refCount > 0)
		{
			controlBlock.create(storeEntry);
			return;
		}
		else
		{
			arc_CHECK_Assert(!(*waiters)->continuations.size() && !(*waiters)->callbacks.size());
		}
	}

	arc_CHECK_Precondition(!controlBlock.promiseBase);

	arc::detail::promise_base & promise = *controlBlock.promiseBase;

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

void arc::detail::store::set_empty_once_callback(
	std::move_only_function<void()> && emptyOnceCallback)
{
	if (!emptyOnceCallback)
		return;

	auto dataHandle = data.ReadAndWrite();

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

	ctx.scheduler.unused(std::move(coroHandle));
}

bool arc::detail::control_block::try_add_continuation(std::coroutine_handle<> continuation)
{
	auto comp = waiters.ReadAndWrite();
	if (!comp->has_value())
		return false;
	(*comp)->continuations.push_back(continuation);
	return true;
}

bool arc::detail::control_block::try_add_callback(std::move_only_function<void()> && callback)
{
	arc_CHECK_Precondition(callback);

	auto comp = waiters.ReadAndWrite();
	if (!comp->has_value())
		return false;
	(*comp)->callbacks.emplace_back(std::move(callback));
	return true;
}

arc::detail::control_block::~control_block()
{
	arc_CHECK_Precondition(referenceCount.load(std::memory_order::relaxed) == 0);
	arc_CHECK_Precondition(value == nullptr);

	if (value)
		deleter(value);
}

arc::detail::promise_base::~promise_base()
{
	selfHandle->second.ctx.name_store().set_name(self, nullptr);
	selfHandle->second.promiseBase = nullptr;
}

void arc::detail::promise_base::unhandled_exception() noexcept
{
	selfHandle->second.exception = std::current_exception();
	arc_DEBUG_Inspect(selfHandle->second.exception);
}

void arc::detail::promise_base::conditionally_complete()
{
	auto comp = selfHandle->second.waiters.ReadAndWrite();

	if (!comp->has_value())
		return;

	for (std::coroutine_handle<> handle : (*comp)->continuations)
		selfHandle->second.ctx.schedule_on_worker_thread(handle);

	for (std::move_only_function<void()> & callback : (*comp)->callbacks)
		callback();

	comp->reset();
}

arc::context::context()
	: context{ arc::options{} }
{}

arc::context::context(const arc::options & options)
	: options_{ options }
	, scheduler{ names, options.mainThreadId, options.workerThreadCount }
{}

const arc::options & arc::context::options() const { return options_; }

arc::detail::name_store & arc::context::name_store() { return names; }

void arc::context::schedule_on_worker_thread(std::coroutine_handle<> handle)
{
	return scheduler.schedule(handle, std::nullopt, false);
}

void arc::context::schedule_on_worker_thread_after(
	std::coroutine_handle<> handle, arc::time_point timePoint)
{
	return scheduler.schedule(handle, timePoint, false);
}

void arc::context::schedule_on_worker_thread(std::move_only_function<void()> && task)
{
	return scheduler.schedule(std::move(task), false);
}

void arc::context::schedule_on_main_thread(std::coroutine_handle<> handle)
{
	return scheduler.schedule(handle, std::nullopt, true);
}

void arc::context::schedule_on_main_thread_after(
	std::coroutine_handle<> handle, arc::time_point timePoint)
{
	return scheduler.schedule(handle, timePoint, true);
}

void arc::context::schedule_on_main_thread(std::move_only_function<void()> && task)
{
	return scheduler.schedule(std::move(task), true);
}

arc::options arc::options::hardware_concurrency()
{
	return {
		.workerThreadCount = std::max<unsigned int>(std::thread::hardware_concurrency(), 2) - 1,
		.mainThreadId = std::this_thread::get_id(),
	};
}

arc::options arc::options::hardware_concurrency_no_main_thread()
{
	return {
		.workerThreadCount = std::max<unsigned int>(std::thread::hardware_concurrency(), 2) - 1,
	};
}

arc::options arc::options::two_threads()
{
	return {
		.workerThreadCount = 1,
		.mainThreadId = std::this_thread::get_id(),
	};
}

std::span<const char * const> arc::options::make_args(int argc, char * argv[])
{
	if (argc > 0)
		return { const_cast<const char **>(argv), size_t(argc) };
	else
		return {};
}

std::vector<const char *> arc::options::make_args(
	std::span<const char *> baseArgs, int argc, char * argv[])
{
	std::vector<const char *> args;

	std::span<const char * const> cmdArgs = make_args(argc, argv);

	args.reserve(cmdArgs.size() + baseArgs.size());

	for (const char * a : cmdArgs)
		args.emplace_back(a);

	for (const char * a : baseArgs)
		args.emplace_back(a);

	return args;
}

arc::detail::globals::~globals()
{
	auto globalsIt = store.ReadAndWrite();
	while (globalsIt->size())
		globalsIt->pop();
}

void arc::detail::globals::add(arc::detail::handle && global)
{
	store.ReadAndWrite()->push(std::move(global));
}

arc::detail::store::store()
{
	arc_TRACE_CONTAINER_CONFIGURE(data.ReadAndWrite()->store, "arc::detail::store");
}

arc::detail::store::~store() { arc_CHECK_Precondition(!data.ReadAndWrite()->store.size()); }
