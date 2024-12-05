#pragma once

#include "arc/extra/non_copyable_non_movable.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::detail::scheduler : private arc::extra::non_copyable_non_movable
{
private:
	struct CoroScheduleOperation
	{
	public:
		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> awaiter) const noexcept
		{
			scheduler.schedule(awaiter, timePoint, mainThread);
		}

		void await_resume() const noexcept {}

		arc::detail::scheduler & scheduler;
		std::optional<arc::time_point> timePoint;
		bool mainThread = false;
	};

public:
	scheduler(
		arc::detail::name_store & names, std::thread::id mainThreadId, size_t workerThreadCount);

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
		return CoroScheduleOperation{ *this, timePoint, mainThread };
	}

	void schedule(
		std::coroutine_handle<> awaiter, const std::optional<arc::time_point> & timePoint,
		bool mainThread);

	void schedule(std::move_only_function<void()> && task, bool mainThread);

	void unused(arc::detail::handle && handle);

private:
	void worker(std::stop_token stopToken, std::optional<size_t> workerIndex);

	/** Thread-safe: No. */
	void start_workers(size_t count);

private:
	struct ArcSchedulerWorkPool
	{
		arc_TRACE_CONDITION_VARIABLE_ANY cv;
		arc_TRACE_LOCKABLE(std::mutex, mtx, "ArcSchedulerWorkPoolMtx");
		arc_TRACE_CONTAINER_QUEUE(std::coroutine_handle<>) work;
		std::vector<std::pair<arc::time_point, std::coroutine_handle<>>> timers;

		size_t unusedCachesize = 0;
		arc_TRACE_CONTAINER_QUEUE(arc::detail::handle) potentiallyUnused;
		arc_TRACE_CONTAINER_QUEUE(std::move_only_function<void()>) tasks;
	};

private:
	arc::detail::name_store & names;

	ArcSchedulerWorkPool workerThreadWork;
	ArcSchedulerWorkPool mainThreadWork;

	std::vector<std::thread> workers;
	std::stop_source stopSource;
	std::thread::id mainThreadId;
};
