#pragma once

#include "arc/util/std.hpp"

namespace arc
{
	struct context;

	template <typename T>
	struct future;

	template <typename T>
	struct result;

	template <typename T>
	struct coro;

	struct tile;

	struct options;

	template <typename T>
	struct all;

	template <typename T>
	struct funnel;

	template <typename T>
	struct promise_proxy;

	using clock = std::chrono::steady_clock;

	using time_point = clock::time_point;

	template <typename T>
	auto get_self_reference();

	template <typename T>
	auto get_promise_proxy();
}

namespace arc::detail
{
	template <typename T>
	struct remove_coro
	{
		using type = T;
	};

	template <typename T>
	struct remove_coro<arc::coro<T>>
	{
		using type = T;
	};

	template <typename T>
	using remove_coro_t = typename remove_coro<T>::type;
}

namespace arc::detail
{
	template <typename>
	struct reflect_function;
}

namespace arc
{
	template <typename F>
	using result_of_t = detail::remove_coro_t<
		typename detail::reflect_function<std::remove_cvref_t<F>>::return_type>;

	template <typename F, size_t I>
	struct key_of;

	template <typename F, size_t I>
	using key_of_t = typename key_of<F, I>::type;

	template <typename F>
	inline constexpr size_t key_count_of_v =
		arc::detail::reflect_function<std::remove_cvref_t<F>>::argument_count - 1;
}

namespace arc::extra
{
	template <typename T>
	struct task;

	template <typename... Args>
	arc::extra::task<std::tuple<arc::result<typename Args::element_type>...>> all(
		Args &&... futures);

	template <typename F>
	struct on_scope_exit;

	struct non_copyable_non_movable;

	template <typename Q, typename T = typename Q::value_type>
	T queue_pop(Q & queue);

	template <typename T, typename Pred>
	typename std::vector<T>::iterator insert_sorted(
		std::vector<T> & vec, const T & item, Pred pred);

	template <typename T>
	typename std::vector<T>::iterator insert_sorted(std::vector<T> & vec, const T & item);

	constexpr uint32_t FNV_1a_32(std::string_view str, uint32_t hash = 2166136261u);

	template <typename T>
	struct shared_guard;

	template <typename T>
	struct recursive_guard;
}

namespace arc::detail
{
	template <typename T>
	struct is_coro
	{
		static constexpr bool value = false;
	};

	template <typename T>
	struct is_coro<arc::coro<T>>
	{
		static constexpr bool value = true;
	};

	template <typename T>
	inline constexpr bool is_coro_v = is_coro<T>::value;
}

namespace arc::detail
{
	struct handle;

	struct scheduler;

	struct store;

	struct promise_base;

	struct control_block;

	struct globals;

	struct name_store;

	struct key;

	template <typename T>
	struct promise;

	using global = std::monostate;

	using store_entry = std::pair<const key, control_block>;

	template <auto>
	struct reflect_pointer_to_member;

	template <typename F, size_t keyCount = arc::key_count_of_v<F>>
	void create_shared_task(arc::detail::store_entry & storeEntry);
}

namespace arc::extra::detail
{
	template <typename T>
	struct task_traits;

	template <typename T>
	struct task_promise_base;

	template <typename T>
	struct task_promise;

	template <typename T, typename M, bool IsConst, bool WithSharedLock>
	struct guard_lock_pointer;
}
