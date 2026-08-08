#pragma once

#include "arc/arc/coro.hpp"
#include "arc/arc/future.hpp"
#include "arc/arc/key_of.hpp"
#include "arc/arc/options.hpp"
#include "arc/arc/promise_proxy.hpp"
#include "arc/detail/globals.hpp"
#include "arc/detail/scheduler.hpp"
#include "arc/detail/store.hpp"
#include "arc/util/non_copyable_non_movable.hpp"
#include "arc/util/util.hpp"

#include <coroutine>
#if arc_WITH_SOURCE_LOCATION
	#include <source_location>
#endif

namespace arc
{
	struct context;
}

namespace arc
{
	template <typename T>
	struct task;

	template <typename T>
	class task_future;
}

struct arc::context
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(context);

	context();

	context(const arc::options & options);

	~context();

	auto schedule_on_worker_thread();
	void schedule_on_worker_thread(std::coroutine_handle<> handle);
	void schedule_on_worker_thread(arc::function<void()> && task, arc::detail::zone_info zone);
	auto schedule_on_worker_thread_after(arc::time_point timePoint);
	void schedule_on_worker_thread_after(std::coroutine_handle<> handle, arc::time_point timePoint);
	void schedule_on_worker_thread_after(
		arc::function<void()> && task, arc::time_point timePoint, arc::detail::zone_info zone);

	auto schedule_on_main_thread();
	void schedule_on_main_thread(std::coroutine_handle<> handle);
	void schedule_on_main_thread(arc::function<void()> && task, arc::detail::zone_info zone);
	auto schedule_on_main_thread_after(arc::time_point timePoint);
	void schedule_on_main_thread_after(std::coroutine_handle<> handle, arc::time_point timePoint);
	void schedule_on_main_thread_after(
		arc::function<void()> && task, arc::time_point timePoint, arc::detail::zone_info zone);

	/**
	 * \defgroup Set Caching Policy Global Defers the destruction of the result until arc::context
	 * is destroyed. This does not guarantee that the result will not be recreated during context
	 * destruction. This might happen for example due to another results destructor requesting the
	 * global again after it has been released.
	 * @{
	 */
	template <typename T>
	void set_caching_policy_global(arc::future<T> global);
	template <typename T>
	void set_caching_policy_global(arc::result<T> global);
	/** @} */

	const arc::options & options() const;

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f, arc::key_of_t<F, 0> key0
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1, arc::key_of_t<F, 2> key2
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1, arc::key_of_t<F, 2> key2,
		arc::key_of_t<F, 3> key3
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	template <typename F>
	arc::future<arc::result_of_t<F>> operator[](
		F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1, arc::key_of_t<F, 2> key2,
		arc::key_of_t<F, 3> key3, arc::key_of_t<F, 4> key4
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation = std::source_location::current()
#endif
	);

	friend bool operator==(const context & lhs, const context & rhs) noexcept
	{
		return &lhs == &rhs;
	}

private:
	template <typename F>
	friend struct arc::detail::key_impl;

	friend arc::detail::control_block;

	template <typename T>
	friend struct future;

	template <typename T>
	friend struct arc::task;

	template <typename T>
	friend class arc::task_future;

private:
	arc::options options_;
	arc::detail::store store;
	/**
	 *  NOTE: The arc::detail::scheduler must be destroyed before most of the other members of
	 *        arc::context, because of that it is placed in this location.
	 */
	arc::detail::scheduler scheduler;
	/** NOTE: arc::detail::globals must be destroyed even before arc::detail::scheduler. */
	arc::detail::globals globals;
};

namespace arc::util
{
	template <typename Hash>
	void hash_append(Hash & hash, const arc::context & value)
	{
		hash_append(hash, reinterpret_cast<uintptr_t>(&value));
	}
}
