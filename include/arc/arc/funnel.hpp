#pragma once

#include "arc/arc/context.hpp"
#include "arc/extra/algorithms.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

template <typename T>
struct arc::funnel
{
	funnel(arc::context & ctx)
		: ctx{ ctx }
	{}

	void push(const arc::future<T> & future)
	{
		future.async_wait_and_then([this](arc::result<T> result) mutable
								   { complete_one(std::move(result)); });
		size_++;
	}

	/** C++ awaitable API */
	bool await_ready() const noexcept { return false; }

	/** C++ awaitable API */
	bool await_suspend(std::coroutine_handle<> awaiter)
	{
		if (auto stateIt = state.ReadAndWrite(); stateIt->ready.size() == 0)
		{
			arc_CHECK_Assert(!stateIt->awaiter);
			stateIt->awaiter = awaiter;
			return true;
		}

		return false;
	}

	/** C++ awaitable API */
	arc::result<T> await_resume()
	{
		auto stateIt = state.ReadAndWrite();

		arc_CHECK_Precondition(stateIt->ready.size());

		arc::result result = arc::extra::queue_pop(stateIt->ready);

		arc_CHECK_Assert(result);

		size_--;

		return result;
	}

	~funnel()
	{
		/** NOTE: because of pending callbacks */
		arc_CHECK_Precondition(!size());
	}

	size_t size() const { return size_; }

private:
	struct State
	{
		std::queue<arc::result<T>> ready;
		std::coroutine_handle<> awaiter;
	};

	arc::context & ctx;
	arc::extra::shared_guard<State> state;
	size_t size_ = 0;

	void complete_one(arc::result<T> && result)
	{
		auto stateIt = state.ReadAndWrite();
		stateIt->ready.push(std::move(result));
		if (stateIt->awaiter)
		{
			arc_CHECK_Assert(stateIt->ready.size() == 1);
			ctx.schedule_on_worker_thread(std::exchange(stateIt->awaiter, nullptr));
		}
	}
};
