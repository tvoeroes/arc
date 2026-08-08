#pragma once

#include "arc/detail/name_store.hpp"
#include "arc/util/non_copyable_non_movable.hpp"
#include "arc/util/util.hpp"

#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#define arc_SCHEDULER_TRACE_LOCK 0

namespace arc::detail
{
	struct scheduler;
}

struct arc::detail::scheduler
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(scheduler);

	struct task
	{
		arc::function<void()> function;
		arc::detail::zone_info zone;
	};

	scheduler(std::thread::id mainThreadId, size_t workerThreadCount);

	~scheduler();

	/** Thread-safe: No. */
	void assist(std::stop_token && stopToken);

	/** Thread-safe: No. */
	void assist();

	/**
	 * Thread-safe: No.
	 *
	 * Can only be called from arc::context (maybe?).
	 */
	void request_stop();

	auto schedule(const std::optional<arc::time_point> & timePoint, bool mainThread)
	{
		struct Awaitable
		{
			bool await_ready() const noexcept { return false; }

			void await_suspend(std::coroutine_handle<> awaiter) const noexcept
			{
				scheduler.schedule(
					task{ awaiter, arc::detail::get_zone_info(awaiter.address()) }, timePoint,
					mainThread);
			}

			void await_resume() const noexcept {}

			arc::detail::scheduler & scheduler;
			std::optional<arc::time_point> timePoint;
			bool mainThread = false;
		};

		return Awaitable{ *this, timePoint, mainThread };
	}

	void schedule(task && task, const std::optional<arc::time_point> & timePoint, bool mainThread);

	void schedule(task && task, bool mainThread, bool highPrio);

	static constexpr bool ArcSchedulerWorkPool_USING_QUEUE = false;

private:
	void worker(std::stop_token stopToken, std::optional<size_t> workerIndex);

	/** Thread-safe: No. */
	void start_workers(size_t count);

private:
	struct work_pool
	{
#if arc_SCHEDULER_TRACE_LOCK
		arc_TRACE_CONDITION_VARIABLE_ANY cv;
		arc_TRACE_LOCKABLE(std::mutex, mtx, "ArcSchedulerWorkPoolMtx");
#else
		std::condition_variable_any cv;
		std::mutex mtx;
#endif

		using timed_task = std::pair<arc::time_point, task>;
		/**
		 * Ties are inserted after all the existing elements. Keep that in mind
		 * if replacing with e.g. std::priority_queue which is not stable.
		 */
		arc_TRACE_CONTAINER_VECTOR(timed_task) timers;

		/** poor man's multi-prio queue */
		arc_TRACE_CONTAINER_QUEUE(task) highPrioTasks;
		arc_TRACE_CONTAINER_STACK(task) tasks;
	};

private:
	work_pool workerThreadWork;
	work_pool mainThreadWork;

	std::vector<std::thread> workers;
	std::stop_source stopSource;
	std::thread::id mainThreadId;
};
