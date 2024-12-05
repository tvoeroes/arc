#pragma once

#include "arc/arc.hpp"
#include "arc/util/check.hpp"

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

template <typename T>
struct arc::extra::detail::task_traits
{
	using result_type = T &&;
	using result_container = std::optional<T>;
};

template <>
struct arc::extra::detail::task_traits<void>
{
	using result_type = void;
	struct result_container
	{
		void operator*() const noexcept {}
		void emplace() const noexcept {}
	};
};

template <typename T>
struct arc::extra::detail::task_promise_base
{
public:
	task_promise_base() = default;

	task_promise_base(task_promise_base const &) = delete;
	task_promise_base & operator=(task_promise_base const &) = delete;
	task_promise_base(task_promise_base &&) = delete;
	task_promise_base & operator=(task_promise_base &&) = delete;

	arc::extra::task<T> get_return_object()
	{
		task_promise<T> & self = static_cast<task_promise<T> &>(*this);
		return arc::extra::task<T>{ std::coroutine_handle<task_promise<T>>::from_promise(self) };
	}

	std::suspend_always initial_suspend() const noexcept { return {}; }

	struct CoroTaskFinalAwaitable
	{
		bool await_ready() const noexcept { return false; }

		template <typename U>
		std::coroutine_handle<> await_suspend(std::coroutine_handle<U> awaiter) const noexcept
		{
			if (auto continuation = awaiter.promise().continuation; continuation)
				return continuation;
			else
				return std::noop_coroutine();
		}

		void await_resume() const noexcept {}
	};

	auto final_suspend() const noexcept { return CoroTaskFinalAwaitable{}; }

	void unhandled_exception() noexcept { exception = std::current_exception(); }

	typename task_traits<T>::result_type result() &&
	{
		if (exception)
			std::rethrow_exception(exception);
		else
		{
			if constexpr (std::is_void_v<typename task_traits<T>::result_type>)
				return;
			else
				return std::move(*returnValue);
		}
	}

	void SetContinuation(std::coroutine_handle<> handle) noexcept { continuation = handle; }

	template <typename... Args>
	void StoreResult(Args &&... value)
	{
		returnValue.emplace(std::forward<Args>(value)...);
	}

private:
	std::coroutine_handle<> continuation;
	std::exception_ptr exception;
	typename task_traits<T>::result_container returnValue;
};

/**
 * The C++ committee dropped the ball here. The only reason for why there is a base class and two
 * derived classes instead of just task_promise is that return_void() and return_value() are not
 * allowed to be implemented in the same class.
 */
template <typename T>
struct arc::extra::detail::task_promise final : public arc::extra::detail::task_promise_base<T>
{
	void return_value(T && value)
	{
		/** dunno why this fails to compile if there is no "this->" maybe compiler bug? */
		this->StoreResult(std::move(value));
	}

	void return_value(const T & value)
	{
		/** dunno why this fails to compile if there is no "this->" maybe compiler bug? */
		this->StoreResult(value);
	}
};

template <>
struct arc::extra::detail::task_promise<void> final : arc::extra::detail::task_promise_base<void>
{
	void return_void() { StoreResult(); }
};

/**
 * Coroutine Task. Compared to other implementations, it is missing support for reference return
 * types.
 */
template <typename T>
struct arc::extra::task
{
public:
	using promise_type = arc::extra::detail::task_promise<T>;

	task() = default;

	task(const task &) = delete;

	task & operator=(const task &) = delete;

	task(task && other) noexcept
		: coroutine{ std::exchange(other.coroutine, nullptr) }
	{}

	task & operator=(task && other) noexcept
	{
		if (this != std::addressof(other))
		{
			if (coroutine)
				coroutine.destroy();
			coroutine = std::exchange(other.coroutine, nullptr);
		}
		return *this;
	}

	~task()
	{
		if (coroutine)
			coroutine.destroy();
	}

	auto operator co_await() const && noexcept
	{
		struct CoroTaskAwaitable
		{
			std::coroutine_handle<promise_type> coroutine;

			bool await_ready() const noexcept { return coroutine.done(); }

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiter) const noexcept
			{
				coroutine.promise().SetContinuation(awaiter);
				return coroutine;
			}

			typename arc::extra::detail::task_traits<T>::result_type await_resume() const
			{
				return std::move(coroutine.promise()).result();
			}
		};
		return CoroTaskAwaitable{ coroutine };
	}

	std::coroutine_handle<> HACK_Handle() const noexcept { return coroutine; }

	void try_resume() const
	{
		if (coroutine && !coroutine.done())
			coroutine.resume();
	}

	bool done() const
	{
		if (!coroutine)
			return false;
		return coroutine.done();
	}

	typename arc::extra::detail::task_traits<T>::result_type result() &&
	{
		arc_CHECK_Precondition(coroutine && coroutine.done());
		return std::move(coroutine.promise()).result();
	}

private:
	std::coroutine_handle<promise_type> coroutine;

	task(std::coroutine_handle<promise_type> coroutine) noexcept
		: coroutine{ coroutine }
	{}

	friend struct arc::extra::detail::task_promise_base<T>;
};

template <typename... Args>
arc::extra::task<std::tuple<arc::result<typename Args::element_type>...>> arc::extra::all(
	Args &&... futures)
{
	co_return std::tuple{ (co_await std::move(futures))... };
}
