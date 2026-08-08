#pragma once

#include "arc/arc/promise_proxy.hpp"
#include "arc/detail/control_block.hpp"
#include "arc/detail/coro_promise_base.hpp"
#include "arc/detail/key.hpp"
#include "arc/detail/name_store.hpp"
#include "arc/util/check.hpp"
#include "arc/util/tracing.hpp"

namespace arc::detail
{
	template <typename T>
	struct coro_promise;
}

/** Forward declaration of coro_promise<T&> partial specialization (reference return type) */
template <typename T>
struct arc::detail::coro_promise<T &> final : arc::detail::coro_promise_base
{
public:
	using handle_type = std::coroutine_handle<coro_promise<T &>>;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	/**
	 * `Tag` defaults to a lambda so every coroutine using this promise instantiates its own
	 * specialization, which is what gives arc_TRACE_MAKE_ZONE_INFO one source location record per
	 * coroutine rather than one per T. `s` is a defaulted parameter so it names the coroutine
	 * rather than this constructor, and is forwarded so the record carries the coroutine's file
	 * and line.
	 *
	 * WARNING: do not drop `Tag`, and do not move this body into a non-template helper. Either
	 * undoes the per-coroutine record and collapses every coroutine returning the same T onto one
	 * name. arc_TRACE_MAKE_ZONE_INFO catches that and degrades to transient zones, so the symptom
	 * is a warning on stderr and a slower profile rather than a wrong one.
	 */
	template <auto Tag = []{}>
	coro_promise(const std::source_location & s = std::source_location::current())
	{
		if (const char * n = s.function_name(); n && *n != '\0')
			arc::detail::set_zone_info(
				handle_type::from_promise(*this).address(), arc_TRACE_MAKE_ZONE_INFO<Tag>(s));
	}
#else
	coro_promise() = default;
#endif

	/** C++ promise API */
	handle_type get_return_object() noexcept { return handle_type::from_promise(*this); }

	/** C++ promise API — stores a non-owning reference; T must outlive the result */
	void return_value(T & value) noexcept
	{
		self_handle_->second.result.emplace_ref(value);
		publish_result();
	}

#if arc_TRACE_INSTRUMENTATION_ENABLE
	~coro_promise() { arc::detail::clear_zone_info(handle_type::from_promise(*this).address()); }
#else
	~coro_promise() = default;
#endif
};

template <typename T>
struct arc::detail::coro_promise final : arc::detail::coro_promise_base
{
public:
	using handle_type = std::coroutine_handle<coro_promise<T>>;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	/** See the coro_promise<T &> constructor for why `Tag` and `s` are shaped this way. */
	template <auto Tag = []{}>
	coro_promise(const std::source_location & s = std::source_location::current())
	{
		if (const char * n = s.function_name(); n && *n != '\0')
			arc::detail::set_zone_info(
				handle_type::from_promise(*this).address(), arc_TRACE_MAKE_ZONE_INFO<Tag>(s));
	}
#else
	coro_promise() = default;
#endif

	/** C++ promise API */
	handle_type get_return_object() noexcept { return handle_type::from_promise(*this); }

	/** C++ promise API */
	void return_value(arc::util::const_removed_t<T> && value) noexcept /** Not exception-safe
																		  therefore noexcept */
	{
		self_handle_->second.result.emplace_value<T>(std::move(value));
		publish_result();
	}

	/** C++ promise API */
	[[deprecated("Consider using return_value(&&)")]] void return_value(
		const arc::util::const_removed_t<T> & value) noexcept
	/** Not exception-safe therefore noexcept */
	{
		self_handle_->second.result.emplace_value<T>(value);
		publish_result();
	}

	/** C++ promise API */
	void return_value(const arc::promise_proxy<T> & value) noexcept /** Not exception-safe
																	   therefore noexcept */
	{
		arc_CHECK_Precondition(
			self_handle_->second.result.holds_value() &&
			self_handle_->second.result.get_value_or_rethrow_exception<T>() &&
			value.handle == self_handle_);
		if (!published_early_)
			publish_result();
	}

#if arc_TRACE_INSTRUMENTATION_ENABLE
	~coro_promise() { arc::detail::clear_zone_info(handle_type::from_promise(*this).address()); }
#else
	~coro_promise() = default;
#endif
};

template <typename T>
struct arc::publish
{
public:
	T * value = nullptr;

	bool await_ready() const noexcept { return false; }

	template <typename Promise>
	bool await_suspend(
		std::coroutine_handle<Promise> handle) noexcept /** Not exception-safe therefore noexcept */
	{
		auto & base = handle.promise();
		arc_CHECK_Precondition(
			!base.published_early_ && base.self_handle_->second.result.holds_value() && value &&
			value == base.self_handle_->second.result.template get_value_or_rethrow_exception<T>());
		base.publish_result();
		base.published_early_ = true;
		return false;
	}

	void await_resume() const noexcept {}
};
