#pragma once

#include "arc/detail/reflect.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"
#include "arc/util/util.hpp"

template <typename F, size_t I>
struct arc::key_of
{
private:
	using function_reflection = arc::detail::reflect_function<std::remove_cvref_t<F>>;

	static_assert(
		function_reflection::argument_count >= 1 || function_reflection::argument_count <= 3);
	static_assert(I <= function_reflection::argument_count - 1);

	using arguments_tuple = function_reflection::argument_tuple;

	using coro_context_type = std::tuple_element_t<0, arguments_tuple>;

	static_assert(
		std::is_same_v<coro_context_type, arc::context &>,
		"First argument must be arc::context &.");

	using arguments_virtual_tuple = std::conditional_t<
		function_reflection::argument_count == 1,
		arc::util::tuple_cat_t<arguments_tuple, std::tuple<const arc::detail::global &>>,
		arguments_tuple>;

	using key_arg_type = typename std::tuple_element_t<I + 1, arguments_virtual_tuple>;

	static_assert(std::is_lvalue_reference_v<key_arg_type>);
	static_assert(std::is_const_v<std::remove_reference_t<key_arg_type>>);

public:
	using type = std::remove_cvref_t<key_arg_type>;
};
