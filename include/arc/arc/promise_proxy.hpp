#pragma once

#include "arc/detail/control_block.hpp"
#include "arc/detail/key.hpp"
#include "arc/fwd.hpp"

template <typename T>
struct arc::promise_proxy
{
public:
	template <typename... Args>
	T * construct(Args &&... args)
	{
		return handle->second.construct<T>(std::forward<Args>(args)...);
	}

private:
	friend struct arc::detail::promise<T>;

	template <typename U>
	friend auto arc::get_promise_proxy();

private:
	promise_proxy(arc::detail::handle && handle)
		: handle{ handle }
	{}

	arc::detail::handle handle;
};
