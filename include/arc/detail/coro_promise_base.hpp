#pragma once

#include "arc/detail/handle.hpp"
#include "arc/util/non_copyable_non_movable.hpp"
#include "arc/util/tracing.hpp"

#include <coroutine>

namespace arc
{
	/**
	 * Awaitable type for early-publish pattern.
	 *
	 * Usage: co_await arc::publish{ &result };
	 *
	 * Publishes the result of a coroutine before it returns, making the result
	 * available to consumers immediately.
	 */
	template <typename T>
	struct publish;
}

namespace arc::detail
{
	struct coro_promise_base;
}

struct arc::detail::coro_promise_base
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(coro_promise_base);

	const arc::detail::handle & handle() { return self_handle_; }

	void set_self_handle(arc::detail::handle && handle) noexcept
	{
		self_handle_ = std::move(handle);
	}

	/** C++ promise API */
	std::suspend_always initial_suspend() const noexcept { return {}; }

	/** C++ promise API */
	std::suspend_never final_suspend() const noexcept { return {}; }

	/** C++ promise API */
	void unhandled_exception() noexcept;

protected:
	coro_promise_base() = default;

	~coro_promise_base();

	void publish_result() noexcept;

	template <typename T>
	friend struct arc::publish;

protected:
	/** For keeping itself alive while computing the result. */
	arc::detail::handle self_handle_;

	bool published_early_ = false;
};
