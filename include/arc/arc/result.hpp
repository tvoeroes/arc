#pragma once

#include "arc/detail/handle.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

/**
 * std::shared_ptr-like smart pointer with deferred deletion for objects that
 * are managed by arc::context. The only reason why this can not be replaced by
 * std::shared_ptr is the deferred deletion feature.
 */
template <typename T>
struct arc::result
{
public:
	using element_type = T;

	/** default constructor */
	result();

	/** default constructor from nullptr */
	result(std::nullptr_t);

	/** move constructor */
	result(result && other);

	/** copy constructor */
	result(const result & other);

	/** up-casting move constructor */
	template <typename U>
	result(result<U> && other);

	/** up-casting copy constructor */
	template <typename U>
	result(const result<U> & other);

	/** aliasing move constructor */
	template <typename U>
	result(T * value, result<U> && other);

	/** aliasing copy constructor */
	template <typename U>
	result(T * value, const result<U> & other);

	/** move assignment */
	result & operator=(result && other);

	/** copy assignment */
	result & operator=(const result & other);

	/** up-casting move assignment */
	template <typename U>
	result & operator=(result<U> && other);

	/** up-casting copy assignment */
	template <typename U>
	result & operator=(const result<U> & other);

	/** resetting assignment */
	result & operator=(std::nullptr_t);

	/** returns stored pointer or nullptr */
	T * get() const noexcept;

	/** returns stored pointer */
	T * operator->() const noexcept;

	/** returns reference to the value pointed to by the stored pointer */
	template <typename T2 = T, std::enable_if_t<!std::is_void_v<T2>, int> = 0>
	T2 & operator*() const noexcept;

	/** returns true if object stores a pointer, false otherwise */
	explicit operator bool() const noexcept;

	/**
	 * returns key. If the type K doesn't match with the key of the managed
	 * object then the behavior is undefined. Note that
	 * arc::key_of_t<F, I> is not necessarily the correct type.
	 */
	template <typename K, size_t I>
	const K & get_key() const;

private:
	template <typename U>
	friend struct arc::result;

	friend arc::future<T>;

	friend arc::context;

	struct impl;

	/** initializing constructor */
	result(T * value, arc::detail::handle && handle) noexcept;

	arc::detail::handle extract_handle() &&;

private:
	T * value = nullptr;
	arc::detail::handle handle;
};
