#pragma once

#include "arc/detail/coro_promise.hpp"
#include "arc/detail/handle.hpp"
#include "arc/detail/reflect.hpp"
#include "arc/util/check.hpp"
#include "arc/util/non_copyable_non_movable.hpp"

#include <coroutine>
#include <type_traits>
#include <utility>

namespace arc
{
	template <typename T>
	struct coro;
}

namespace arc::detail
{
	template <typename T>
	struct is_coro;

	template <typename T>
	inline constexpr bool is_coro_v = is_coro<T>::value;

	template <typename T>
	struct remove_coro;

	template <typename T>
	using remove_coro_t = typename remove_coro<T>::type;
}

namespace arc
{
	template <typename F>
	using result_of_t = detail::remove_coro_t<
		typename detail::reflect_function<std::remove_cvref_t<F>>::return_type>;
}

template <typename T>
struct arc::coro
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(coro);

	using result_type = T;
	using promise_type = arc::detail::coro_promise<T>;
	using handle_type = std::coroutine_handle<promise_type>;

	coro() = delete;

	coro(handle_type handle) noexcept
		: handle{ handle }
	{}

	handle_type extract_handle()
	{
		arc_CHECK_Precondition(handle);
		return std::exchange(handle, nullptr);
	}

	~coro() { arc_CHECK_Precondition(!handle); }

private:
	handle_type handle;
};

template <typename T>
struct arc::detail::is_coro : std::false_type
{};

template <typename T>
struct arc::detail::is_coro<arc::coro<T>> : std::true_type
{};

template <typename T>
struct arc::detail::remove_coro
{
	using type = T;
};

template <typename T>
struct arc::detail::remove_coro<arc::coro<T>>
{
	using type = T;
};
