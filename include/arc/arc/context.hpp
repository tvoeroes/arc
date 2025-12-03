#pragma once

#include "arc/arc/future.hpp"
#include "arc/arc/key_of.hpp"
#include "arc/arc/options.hpp"
#include "arc/arc/promise_proxy.hpp"
#include "arc/detail/globals.hpp"
#include "arc/detail/name_store.hpp"
#include "arc/detail/scheduler.hpp"
#include "arc/detail/store.hpp"
#include "arc/extra/non_copyable_non_movable.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::context : private arc::extra::non_copyable_non_movable
{
public:
	context();

	context(const arc::options & options);

	~context();

	auto schedule_on_worker_thread();
	void schedule_on_worker_thread(std::coroutine_handle<> handle);
	void schedule_on_worker_thread(arc::function<void()> && task);
	auto schedule_on_worker_thread_after(arc::time_point timePoint);
	void schedule_on_worker_thread_after(std::coroutine_handle<> handle, arc::time_point timePoint);

	auto schedule_on_main_thread();
	void schedule_on_main_thread(std::coroutine_handle<> handle);
	void schedule_on_main_thread(arc::function<void()> && task);
	auto schedule_on_main_thread_after(arc::time_point timePoint);
	void schedule_on_main_thread_after(std::coroutine_handle<> handle, arc::time_point timePoint);

	/**
	 * \defgroup Set Caching Policy Global
	 * Defers the destruction of the result until arc::context is destroyed.
	 * This does not guarantee that the result will not be recreated during
	 * context destruction. This might happen for example due to another results
	 * destructor requesting the global again after it has been released.
	 * @{
	 */
	template <typename T>
	void set_caching_policy_global(arc::future<T> global);
	template <typename T>
	void set_caching_policy_global(arc::result<T> global);
	/** @} */

	const arc::options & options() const;

	arc::detail::name_store & name_store();

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(F * f arc_SOURCE_LOCATION_ARG_DEFAULT);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(
		F * f, arc::key_of_t<F, 0> key0 arc_SOURCE_LOCATION_ARG_DEFAULT);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1 arc_SOURCE_LOCATION_ARG_DEFAULT);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1,
		arc::key_of_t<F, 2> key2 arc_SOURCE_LOCATION_ARG_DEFAULT);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1, arc::key_of_t<F, 2> key2,
		arc::key_of_t<F, 3> key3 arc_SOURCE_LOCATION_ARG_DEFAULT);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator()(
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1, arc::key_of_t<F, 2> key2,
		arc::key_of_t<F, 3> key3, arc::key_of_t<F, 4> key4 arc_SOURCE_LOCATION_ARG_DEFAULT);

	friend bool operator==(const context & lhs, const context & rhs) noexcept
	{
		return &lhs == &rhs;
	}

private:
	friend arc::detail::scheduler;

	template <typename F>
	friend struct arc::detail::key_impl;

	friend arc::detail::control_block;

	template <typename T>
	friend struct future;

private:
	arc::options options_;
	/** NOTE: Must be constructed before arc::detail::scheduler */
	arc::detail::name_store names;
	arc::detail::store store;
	/**
	 *  NOTE: The arc::detail::scheduler must be destroyed before most of the other members of
	 *        arc::context, because of that it is placed in this location.
	 */
	arc::detail::scheduler scheduler;
	/** NOTE: arc::detail::globals must be destroyed even before arc::detail::scheduler. */
	arc::detail::globals globals;
};

namespace arc::extra
{
	template <typename Hash>
	void hash_append(Hash & hash, const arc::context & value)
	{
		hash_append(hash, reinterpret_cast<uintptr_t>(&value));
	}
}