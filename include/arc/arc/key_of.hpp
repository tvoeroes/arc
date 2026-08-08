#pragma once

#include "arc/detail/reflect.hpp"
#include "arc/util/util.hpp"

#include <tuple>
#include <type_traits>

namespace arc
{
	template <typename F, size_t I>
	struct key_of;
}

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

	using key_arg_type = typename std::tuple_element_t<I + 1, arguments_tuple>;

	static_assert(std::is_lvalue_reference_v<key_arg_type>);
	static_assert(std::is_const_v<std::remove_reference_t<key_arg_type>>);

public:
	using type = std::remove_cvref_t<key_arg_type>;
};

namespace arc
{
	template <typename F>
	using args_tuple_t = arc::util::remove_tuple_const_reference_t<
		typename arc::detail::reflect_function<std::remove_cvref_t<F>>::argument_tuple>;

	template <typename F>
	inline constexpr size_t key_count_of_v = std::tuple_size_v<args_tuple_t<F>> - 1;

#if 1
	template <typename F, size_t I>
	using key_of_t = typename key_of<F, I>::type;
#else
	template <typename F, size_t I>
	using key_of_t = std::tuple_element_t<I + 1, args_tuple_t<F>>;
#endif
}
