#pragma once

#include "arc/arc/context.hpp"
#include "arc/extra/algorithms.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

template <typename T>
struct arc::all
{
public:
	all() = delete;

	all(arc::context & ctx, std::span<arc::future<T>> futures)
		: ctx{ ctx }
		, futures{ futures }
		, results{ futures.size() }
	{
		for (size_t i = 0; i < futures.size(); i++)
		{
			if (futures[i])
				futures[i].async_wait_and_then([this, i](arc::result<T> r) {
					results[i] = std::move(r);
					arc_CHECK_Assert(results[i]);
					complete_one();
				});
			else
				complete_one();
		}
	}

	/** C++ awaitable API */
	bool await_ready() const noexcept { return false; }

	/** C++ awaitable API */
	void await_suspend(std::coroutine_handle<> awaiter_)
	{
		awaiter = awaiter_;
		complete_one();
	}

	/** C++ awaitable API */
	std::vector<arc::result<T>> await_resume()
	{
		arc_CHECK_Precondition(ready());

		return std::move(results);
	}

private:
	arc::context & ctx;
	std::span<arc::future<T>> futures;
	std::vector<arc::result<T>> results;
	std::atomic_size_t doneCount = 0;
	std::coroutine_handle<> awaiter;

	bool ready() const { return doneCount.load(std::memory_order::acquire) == results.size() + 1; }

	void complete_one()
	{
		size_t currentDoneCount = doneCount.fetch_add(1, std::memory_order::acq_rel) + 1;

		if (currentDoneCount != results.size() + 1)
			return;

		ctx.schedule_on_worker_thread(awaiter);
	}
};
