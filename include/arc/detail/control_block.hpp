#pragma once

#include "arc/arc/future.hpp"
#include "arc/detail/coro_promise_base.hpp"
#include "arc/detail/handle.hpp"
#include "arc/detail/result_store.hpp"
#include "arc/detail/zone_info.hpp"
#include "arc/util/check.hpp"
#include "arc/util/debug.hpp"
#include "arc/util/guard.hpp"
#include "arc/util/non_copyable_non_movable.hpp"
#include "arc/util/util.hpp"

#include <atomic>
#include <optional>
#include <vector>

#if arc_TRACE_INSTRUMENTATION_ENABLE
	#include <source_location>
#endif

namespace arc::detail
{
	/**
	 * NOTE: This class uses the scary raw new+delete.
	 */
	struct control_block;
}

struct arc::detail::control_block
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(control_block);

	control_block() = default;

	~control_block();

	arc::detail::result_store result;

private:
	template <typename T>
	friend struct arc::future;

	friend arc::detail::store;
	friend arc::detail::handle;
	friend arc::detail::coro_promise_base;

	template <typename F>
	friend struct arc::detail::key_impl;

	void add_reference() noexcept;
	void remove_reference(arc::detail::handle && coroHandle);

	bool is_done() const { return !waiters.read_only()->has_value(); }

	/**
	 * \returns true if the continuation was scheduled. False means that the window for signaling
	 *          continuations has passed and that the continuation should be handled by the caller
	 *          of this function instead.
	 */
	bool try_add_continuation(arc::function<void()> && continuation, arc::detail::zone_info zone);

	struct Waiters
	{
		struct Continuation
		{
			arc::function<void()> function;
			arc::detail::zone_info zone;
		};
		std::vector<Continuation> continuations;
	};

private:
	std::atomic_size_t referenceCount{ 0 };

	arc::util::shared_guard<std::optional<Waiters>> waiters{ std::in_place };
#if arc_TRACE_INSTRUMENTATION_ENABLE
	/**
	 * HACK: this guarded by dataHandle lock
	 */
	std::vector<std::source_location> requestLocations;
#endif
};
