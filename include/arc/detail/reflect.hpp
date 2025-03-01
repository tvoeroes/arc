#pragma once

#include <tuple>

template <typename Return, typename... Args>
struct arc::detail::reflect_function<Return(Args...)>
{
	using return_type = Return;

	static constexpr size_t argument_count = sizeof...(Args);

	using argument_tuple = std::tuple<Args...>;

	template <size_t Index>
	using argument_type = std::tuple_element_t<Index, argument_tuple>;
};

template <typename U, typename V, U V::* M>
struct arc::detail::reflect_pointer_to_member<M>
{
	using class_type = V;
	using member_type = U;
	static constexpr U V::* member_pointer = M;
};
