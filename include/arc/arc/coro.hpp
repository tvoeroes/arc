#pragma once

#include "arc/detail/handle.hpp"
#include "arc/detail/promise.hpp"
#include "arc/extra/non_copyable_non_movable.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

template <typename T>
struct arc::coro : private arc::extra::non_copyable_non_movable
{
public:
	using result_type = T;
	using promise_type = arc::detail::promise<T>;
	using handle_type = std::coroutine_handle<promise_type>;

	coro(handle_type handle) noexcept
		: handle{ handle }
	{}

	~coro() { arc_CHECK_Precondition(!handle); }

private:
	coro() = delete;

	operator handle_type()
	{
		arc_CHECK_Precondition(handle);
		return std::exchange(handle, nullptr);
	}

	template <typename F>
	friend struct arc::detail::key_impl;

private:
	handle_type handle;
};
