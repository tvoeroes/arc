#pragma once

#include "arc/util/check.hpp"
#include "arc/util/debug.hpp"

namespace arc::detail
{
	struct result_store;
}

struct arc::detail::result_store
{
public:
	template <typename T, typename... Args>
	T & emplace_value(Args &&... args)
	{
		arc_CHECK_Precondition(holds_nothing());
		T * value = new T{ std::forward<Args>(args)... };
		result.emplace<result_type<T>>(static_cast<void_ptr<T>>(value), [](void_ptr<T> value) {
			delete reinterpret_cast<T *>(value);
		});
		return *value;
	}

	/**
	 * Stores a reference to an existing object (no allocation, no-op deleter).
	 * T must be the referent type (not a reference type itself).
	 * The referenced object must outlive this result_store.
	 */
	template <typename T>
	T & emplace_ref(T & value)
	{
		arc_CHECK_Precondition(holds_nothing());
		result.emplace<result_type<T>>(static_cast<void_ptr<T>>(&value), [](void_ptr<T>) {});
		return value;
	}

	void set_unhandled_exception(std::exception_ptr && exception)
	{
		arc_CHECK_Precondition(holds_nothing());
		result.emplace<std::exception_ptr>(std::move(exception));
		arc_DEBUG_Inspect(std::get<std::exception_ptr>(result));
	}

	/**
	 * \tparam T Must be equal to T in emplace_value<T, ...>() or void. Not even
	 *         base class of T is valid!
	 */
	template <typename T>
	T * get_value_or_rethrow_exception() const
	{
		if (auto * exception = std::get_if<std::exception_ptr>(&result))
			std::rethrow_exception(*exception);
		arc_CHECK_Precondition(std::holds_alternative<result_type<T>>(result));
		return static_cast<T *>(std::get<result_type<T>>(result).get());
	}

	void reset()
	{
		arc_CHECK_Precondition(!holds_nothing());
		result.emplace<std::monostate>();
	}

	bool holds_value() const
	{
		return std::holds_alternative<mut_result_type>(result) ||
			std::holds_alternative<const_result_type>(result);
	}

	bool holds_exception() const { return std::holds_alternative<std::exception_ptr>(result); }

	bool holds_nothing() const { return std::holds_alternative<std::monostate>(result); }

private:
	using mut_result_type = std::unique_ptr<void, void (*)(void *)>;
	using const_result_type = std::unique_ptr<const void, void (*)(const void *)>;
	template <typename T>
	using result_type = std::conditional_t<std::is_const_v<T>, const_result_type, mut_result_type>;
	template <typename T>
	using void_ptr = arc::util::const_matching_void_t<T> *;

private:
	std::variant<std::monostate, mut_result_type, const_result_type, std::exception_ptr> result;
};
