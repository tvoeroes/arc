#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <variant>

namespace arc
{
	struct context;

	template <typename... Signature>
#if __cpp_lib_move_only_function >= 202110L
	using function = std::move_only_function<Signature...>;
	#define arc_FUNCTION_IS_MOVE_ONLY 1
#else
	using function = std::function<Signature...>;
	#define arc_FUNCTION_IS_MOVE_ONLY 0
#endif

	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
}

namespace arc::util
{
	template <class T, class... Ts>
	struct is_any_of : std::disjunction<std::is_same<T, Ts>...>
	{};

	template <class T, class... Ts>
	static constexpr bool is_any_of_v = arc::util::is_any_of<T, Ts...>::value;

	template <typename T, bool B>
	static constexpr bool dependent_bool_v = B;

	template <auto V, bool B>
	static constexpr bool value_dependent_bool_v = B;

	template <typename T>
	struct type_tag
	{};

	template <auto V>
	struct value_tag
	{};

	template <typename T>
	struct is_optional : std::false_type
	{};

	template <typename T>
	struct is_optional<std::optional<T>> : std::true_type
	{};

	template <typename T>
	static constexpr bool is_optional_v = is_optional<T>::value;

	template <typename... Args>
	using tuple_cat_t = decltype(std::tuple_cat(std::declval<Args>()...));

	template <typename T>
	using const_removed_t = std::conditional_t<std::is_const_v<T>, std::remove_const_t<T>, T>;

	template <typename T>
	using const_matching_void_t =
		std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const void, void>;

	template <typename Tuple>
	struct remove_tuple_const_reference;

	template <typename... Args>
	struct remove_tuple_const_reference<std::tuple<arc::context &, Args...>>
	{
		using type =
			std::tuple<arc::context &, std::remove_const_t<std::remove_reference_t<Args>>...>;
	};

	template <typename Tuple>
	using remove_tuple_const_reference_t = typename remove_tuple_const_reference<Tuple>::type;

	template <typename T>
	concept scoped_enum = std::is_scoped_enum_v<T>;

	template <typename... Args>
	struct visit_overloads : Args...
	{
		using Args::operator()...;
	};

	template <typename... Visitors>
	inline auto visit(auto && value, Visitors &&... visitors)
	{
		return std::visit(
			visit_overloads<Visitors...>{ std::forward<Visitors>(visitors)... },
			std::forward<decltype(value)>(value));
	}
}
