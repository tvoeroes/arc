#pragma once

#include "arc/detail/handle.hpp"
#include "arc/extra/non_copyable_non_movable.hpp"
#include "arc/util/std.hpp"
#include "arc/util/tracing.hpp"

struct arc::detail::promise_base : private arc::extra::non_copyable_non_movable
{
public:
	const arc::detail::handle & handle() { return selfHandle; }

	void set_self_handle(arc::detail::handle && handle) noexcept { selfHandle = std::move(handle); }

	void conditionally_complete();

	/** C++ promise API */
	std::suspend_always initial_suspend() const noexcept { return {}; }

	/** C++ promise API */
	void unhandled_exception() noexcept;

protected:
	template <typename F, size_t keyCount>
	friend void arc::detail::create_shared_task(arc::detail::store_entry & storeEntry);

protected:
	promise_base() = default;

	~promise_base();

protected:
	std::coroutine_handle<> self;
	arc::detail::handle selfHandle;
#ifdef arc_TRACE_INSTRUMENTATION_ENABLE
	static constexpr char FALLBACK_NAME[] = "[?]";
	const char * name = FALLBACK_NAME;
#endif
};
