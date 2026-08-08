#pragma once

#include "arc/detail/control_block.hpp"
#include "arc/detail/key.hpp"

namespace arc::detail
{
	template <typename T>
	struct coro_promise;
}

namespace arc
{
	template <typename T>
	auto get_promise_proxy();

	template <typename T>
	struct promise_proxy;
}

template <typename T>
struct arc::promise_proxy
{
public:
	template <typename... Args>
	T & construct(Args &&... args)
	{
		return handle->second.result.emplace_value<T>(std::forward<Args>(args)...);
	}

private:
	friend struct arc::detail::coro_promise<T>;

	template <typename U>
	friend auto arc::get_promise_proxy();

	promise_proxy(arc::detail::handle && handle)
		: handle{ handle }
	{}

private:
	arc::detail::handle handle;
};
