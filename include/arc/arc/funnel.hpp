#pragma once

#include "arc/arc.hpp"
#include "arc/arc/task.hpp"
#include "arc/util/algorithms.hpp"
#include "arc/util/check.hpp"

#include <coroutine>
#include <queue>
#include <utility>

namespace arc
{
	template <typename T>
	struct funnel;
}

template <typename T>
struct arc::funnel<arc::future<T>>
{
public:
	template <typename... F>
	funnel(F &&... schedule)
		: schedule{ std::forward<F>(schedule)... }
	{}

	void push(const arc::future<T> & future)
	{
		future.async_wait_and_then(
			[this](arc::result<T> result) mutable { complete_one(std::move(result)); });
		size_++;
	}

	/** C++ awaitable API */
	bool await_ready() const noexcept { return false; }

	/** C++ awaitable API */
	bool await_suspend(std::coroutine_handle<> awaiter)
	{
		if (auto stateIt = state.read_and_write(); stateIt->ready.size() == 0)
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
		auto stateIt = state.read_and_write();

		arc_CHECK_Precondition(stateIt->ready.size());

		arc::result<T> result = arc::util::queue_pop(stateIt->ready);

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

	void complete_one(arc::result<T> && result)
	{
		auto stateIt = state.read_and_write();
		stateIt->ready.push(std::move(result));
		if (stateIt->awaiter)
		{
			arc_CHECK_Assert(stateIt->ready.size() == 1);
			schedule(std::exchange(stateIt->awaiter, nullptr));
		}
	}

private:
	arc::function<void(std::coroutine_handle<>)> schedule;
	arc::util::shared_guard<State> state;
	size_t size_ = 0;
};

template <typename T>
struct arc::funnel<arc::task<T>>
{
public:
	template <typename... F>
	funnel(F &&... schedule)
		: schedule{ std::forward<F>(schedule)... }
	{}

	void push(arc::task<T> && task_obj, arc::context & ctx, bool onMainThread)
	{
		std::move(task_obj).async_launch_and_async_wait_and_then(
			ctx, onMainThread,
			[this](arc::task_result<T> result) { complete_one(std::move(result)); });
		size_++;
	}

	/** C++ awaitable API */
	bool await_ready() const noexcept { return false; }

	/** C++ awaitable API */
	bool await_suspend(std::coroutine_handle<> awaiter)
	{
		if (auto stateIt = state.read_and_write(); stateIt->ready.size() == 0)
		{
			arc_CHECK_Assert(!stateIt->awaiter);
			stateIt->awaiter = awaiter;
			return true;
		}

		return false;
	}

	/** C++ awaitable API */
	T await_resume()
	{
		auto stateIt = state.read_and_write();

		arc_CHECK_Precondition(stateIt->ready.size());

		arc::task_result<T> result = arc::util::queue_pop(stateIt->ready);

		size_--;

		return result.get();
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
		std::queue<arc::task_result<T>> ready;
		std::coroutine_handle<> awaiter;
	};

	void complete_one(arc::task_result<T> && result)
	{
		auto stateIt = state.read_and_write();
		stateIt->ready.push(std::move(result));
		if (stateIt->awaiter)
		{
			arc_CHECK_Assert(stateIt->ready.size() == 1);
			schedule(std::exchange(stateIt->awaiter, nullptr));
		}
	}

private:
	arc::function<void(std::coroutine_handle<>)> schedule;
	arc::util::shared_guard<State> state;
	size_t size_ = 0;
};
