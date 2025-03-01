#pragma once

#include "arc/fwd.hpp"

#include <optional>
#include <tuple>
#include <type_traits>

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
	using const_matching_void_t = std::conditional_t<std::is_const_v<T>, const void, void>;

	template <typename T>
	const_removed_t<T> * remove_const(std::type_identity_t<T> * ptr)
	{
		return const_cast<const_removed_t<T> *>(ptr);
	}
}
