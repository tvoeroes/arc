#pragma once

#include "arc/util/tracing.hpp"

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

template <typename T, typename M, bool IsConst, bool WithSharedLock>
struct arc::extra::detail::guard_lock_pointer
{
public:
	using reference_type = typename std::conditional_t<IsConst, const T &, T &>;
	using pointer_type = typename std::conditional_t<IsConst, const T *, T *>;
	using lock_type = typename std::conditional_t<
		IsConst && WithSharedLock, std::shared_lock<M>, std::unique_lock<M>>;

	template <bool Const = IsConst>
	std::enable_if_t<Const, pointer_type> operator->() const
	{
		return ptr;
	}

	template <bool Const = IsConst>
	std::enable_if_t<Const, reference_type> operator*() const
	{
		return *ptr;
	}

	template <bool Const = IsConst>
	std::enable_if_t<!Const, pointer_type> operator->()
	{
		return ptr;
	}

	template <bool Const = IsConst>
	std::enable_if_t<!Const, reference_type> operator*()
	{
		return *ptr;
	}

	void Unlock()
	{
		lock.unlock();
		ptr = nullptr;
	}

private:
	guard_lock_pointer(pointer_type ptr, M & mutex)
		: ptr{ ptr }
		, lock{ mutex }
	{}

	pointer_type ptr = nullptr;
	lock_type lock;

	friend struct arc::extra::shared_guard<T>;
	friend struct arc::extra::recursive_guard<T>;
};

template <typename T>
struct arc::extra::shared_guard
{
public:
	using mutex_type = std::shared_mutex;

	shared_guard() = default;

	template <typename... Args>
	shared_guard(Args &&... args)
		: val{ std::forward<Args>(args)... }
	{}

	using iterator_type = arc::extra::detail::guard_lock_pointer<T, mutex_type, false, true>;
	using iterator_const_type = arc::extra::detail::guard_lock_pointer<T, mutex_type, true, true>;

	iterator_type ReadAndWrite() { return { &val, mutex }; }

	iterator_const_type ReadOnly() const { return { &val, mutex }; }

private:
	T val;
	mutable mutex_type mutex;
};

#define arc_TRACE_RECURSIVE_GUARD 0

template <typename T>
struct arc::extra::recursive_guard
{
private:
	using mutex_type = std::recursive_mutex;
#if arc_TRACE_RECURSIVE_GUARD
	using actual_mutex_type = arc_TRACE_LOCKABLE_SHARED_TYPE(mutex_type);
#else
	using actual_mutex_type = mutex_type;
#endif

public:
	recursive_guard() = default;

	template <typename... Args>
	recursive_guard(Args &&... args)
		: val{ std::forward<Args>(args)... }
	{}

	using iterator_type =
		arc::extra::detail::guard_lock_pointer<T, actual_mutex_type, false, false>;
	using iterator_const_type =
		arc::extra::detail::guard_lock_pointer<T, actual_mutex_type, true, false>;

	iterator_type ReadAndWrite() { return { &val, mutex }; }

	/**
	 * NOTE: Unlike shared_guard::ReadOnly(), this ReadOnly() does have no performance benefit over
	 *       ReadAndWrite().
	 */
	iterator_const_type ReadOnly() const { return { &val, mutex }; }

private:
	T val;
#if arc_TRACE_RECURSIVE_GUARD
	mutable arc_TRACE_LOCKABLE_SHARED(mutex_type, mutex, "recursive_guard");
#else
	mutable mutex_type mutex;
#endif
};
