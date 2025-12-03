#pragma once

#include "arc/detail/handle.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"
#include "arc/util/util.hpp"

template <typename T>
struct arc::future
{
public:
	using element_type = T;

	/** default constructor */
	future();

	/** default constructor from nullptr */
	future(std::nullptr_t);

	/** move constructor */
	future(future && other);

	/** copy constructor */
	future(const future & other);

	/** up-casting move constructor, converting from non-original type is undefined behavior */
	template <typename U>
	future(future<U> && other);

	/** up-casting copy constructor, converting from non-original type is undefined behavior */
	template <typename U>
	future(const future<U> & other);

	/** member aliasing move constructor, converting from non-original type is undefined behavior */
	template <auto M, typename U>
	future(arc::util::value_tag<M>, future<U> && other);

	/** member aliasing copy constructor, converting from non-original type is undefined behavior */
	template <auto M, typename U>
	future(arc::util::value_tag<M>, const future<U> & other);

	/** move assignment */
	future & operator=(future && other);

	/** copy assignment */
	future & operator=(const future & other);

	/** up-casting move assignment, converting from non-original type is undefined behavior */
	template <typename U>
	future & operator=(future<U> && other);

	/** up-casting copy assignment, converting from non-original type is undefined behavior */
	template <typename U>
	future & operator=(const future<U> & other);

	/** resetting assignment */
	future & operator=(std::nullptr_t);

	/** returns true if the object can be awaited on, false otherwise */
	explicit operator bool() const noexcept;

	/**
	 * \defgroup Async Wait And Then
	 * Adds callback to the list of callbacks that will be called when the result is ready. The
	 * callback will be invoked immediately if the result is already ready. Note that the future
	 * instance does not need to be kept alive until the callback has been called and that the
	 * callback will be called regardless of whether there are any instances of future alive when
	 * the result is ready.
	 * @{
	 */
	void async_wait_and_then(arc::function<void()> && callback) const;
	void async_wait_and_then(arc::function<void(arc::result<T>)> && callback) const;
	/** @} */

	/**
	 * Synchronously actively waits for the result and returns result when the
	 * result becomes available. Active wait means that the thread joins the
	 * worker pool while waiting.
	 */
	arc::result<T> active_wait();

	/** Returns result if the result is available, otherwise default constructed result. */
	arc::result<T> try_wait();

	/**
	 * Suspends the caller until the result is available, then reschedules the
	 * caller on a worker thread. Returns result on resumption.
	 */
	auto operator co_await();

private:
	using resolve_type = T * (*)(util::const_matching_void_t<T> *);

	explicit future(arc::detail::handle && handle);

	struct impl;

	friend arc::context;
	friend arc::detail::store;

	template <typename U>
	friend auto arc::get_self_reference();

	template <typename U>
	friend struct future;

	arc::detail::handle extract_handle() &&;

private:
	arc::detail::handle handle;
	resolve_type resolve = nullptr;
};
