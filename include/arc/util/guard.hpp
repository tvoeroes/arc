#pragma once

#include "arc/util/coro_shared_mutex.hpp"
#include "arc/util/tracing.hpp"

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

namespace arc::util::detail
{
	template <typename T, typename M, bool IsConst, bool WithSharedLock>
	struct guard_lock_pointer;
}

namespace arc::util
{
	template <typename T>
	struct shared_guard;

	template <typename T>
	struct recursive_guard;

	template <typename T>
	struct coro_shared_guard;
}

template <typename T, typename M, bool IsConst, bool WithSharedLock>
struct arc::util::detail::guard_lock_pointer
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

	friend struct arc::util::shared_guard<T>;
	friend struct arc::util::recursive_guard<T>;
};

template <typename T>
struct arc::util::shared_guard
{
public:
	using mutex_type = std::shared_mutex;

	template <typename... Args>
	shared_guard(Args &&... args)
		: val{ std::forward<Args>(args)... }
	{}

	using pointer = arc::util::detail::guard_lock_pointer<T, mutex_type, false, true>;
	using const_pointer = arc::util::detail::guard_lock_pointer<T, mutex_type, true, true>;

	pointer read_and_write() { return { &val, mutex }; }

	const_pointer read_only() const { return { &val, mutex }; }

private:
	mutable mutex_type mutex;
	T val;
};

/** expensive */
#define arc_TRACE_RECURSIVE_GUARD 0

template <typename T>
struct arc::util::recursive_guard
{
private:
	using mutex_type = std::recursive_mutex;
#if arc_TRACE_RECURSIVE_GUARD
	using actual_mutex_type = arc_TRACE_LOCKABLE_SHARED_TYPE(mutex_type);
#else
	using actual_mutex_type = mutex_type;
#endif

public:
	template <typename... Args>
	recursive_guard(Args &&... args)
		: val{ std::forward<Args>(args)... }
	{}

	using pointer = arc::util::detail::guard_lock_pointer<T, actual_mutex_type, false, false>;
	using const_pointer = arc::util::detail::guard_lock_pointer<T, actual_mutex_type, true, false>;

	pointer read_and_write() { return { &val, mutex }; }

	/**
	 * NOTE: Unlike shared_guard::read_only(), this read_only() does have no performance benefit
	 *       over read_and_write().
	 */
	const_pointer read_only() const { return { &val, mutex }; }

private:
#if arc_TRACE_RECURSIVE_GUARD
	mutable arc_TRACE_LOCKABLE_SHARED(mutex_type, mutex, "recursive_guard");
#else
	mutable mutex_type mutex;
#endif
	T val;
};

/**
 * A coroutine-aware counterpart to shared_guard. Protects a value of type T with a
 * coro_shared_mutex. Acquiring either lock suspends the calling coroutine instead of blocking
 * the thread when the mutex is contended.
 *
 * Usage:
 *   coro_shared_guard<Foo> guard{ ... };
 *
 *   // In a coroutine:
 *   auto w = co_await guard.read_and_write();  // exclusive lock
 *   w->mutate();
 *   // lock released when w is destroyed
 *
 *   auto r = co_await guard.read_only();      // shared lock
 *   r->inspect();
 */
template <typename T>
struct arc::util::coro_shared_guard
{
public:
	using schedule_fn = coro_shared_mutex::schedule_fn;

	template <bool Exclusive>
	class basic_lock;
	template <bool Exclusive>
	struct basic_awaitable;

	/** RAII guard holding an exclusive (Exclusive=true) or shared (Exclusive=false) lock. */
	template <bool Exclusive>
	class basic_lock
	{
	public:
		using ptr_type = std::conditional_t<Exclusive, T *, const T *>;
		using ref_type = std::conditional_t<Exclusive, T &, const T &>;
		using lock_type = std::conditional_t<
			Exclusive, coro_shared_mutex::exclusive_lock, coro_shared_mutex::shared_lock>;

		ptr_type operator->() noexcept { return ptr_; }
		ref_type operator*() noexcept { return *ptr_; }

		void unlock() noexcept
		{
			lock_.unlock();
			ptr_ = nullptr;
		}

		~basic_lock() noexcept = default;

		basic_lock(basic_lock && other) noexcept
			: ptr_{ std::exchange(other.ptr_, nullptr) }
			, lock_{ std::move(other.lock_) }
		{}

		basic_lock(const basic_lock &) = delete;
		basic_lock & operator=(basic_lock &&) = delete;
		basic_lock & operator=(const basic_lock &) = delete;

	private:
		basic_lock(ptr_type ptr, lock_type lk) noexcept
			: ptr_{ ptr }
			, lock_{ std::move(lk) }
		{}

		ptr_type ptr_ = nullptr;
		lock_type lock_;

		friend struct basic_awaitable<Exclusive>;
	};

	using write_lock = basic_lock<true>;
	using read_lock = basic_lock<false>;

	template <bool Exclusive>
	struct basic_awaitable
	{
		using ptr_type = std::conditional_t<Exclusive, T *, const T *>;
		using inner_type = std::conditional_t<
			Exclusive, coro_shared_mutex::lock_awaitable, coro_shared_mutex::lock_shared_awaitable>;

		bool await_ready() noexcept { return inner_.await_ready(); }

		bool await_suspend(std::coroutine_handle<> h) noexcept { return inner_.await_suspend(h); }

		basic_lock<Exclusive> await_resume() noexcept { return { ptr_, inner_.await_resume() }; }

		ptr_type ptr_;
		inner_type inner_;
	};

	using write_awaitable = basic_awaitable<true>;
	using read_awaitable = basic_awaitable<false>;

	template <typename... Args>
	coro_shared_guard(Args &&... args)
		: val_{ std::forward<Args>(args)... }
	{}

	/** Returns an awaitable that, when co_awaited, acquires an exclusive (write) lock.
	 *  When contended, the schedule function is called with the handle to resume later. */
	write_awaitable read_and_write(const schedule_fn * schedule) noexcept
	{
		return { &val_, mutex_.lock(schedule) };
	}

	/** Returns an awaitable that, when co_awaited, acquires a shared (read) lock.
	 *  When contended, the schedule function is called with the handle to resume later. */
	read_awaitable read_only(const schedule_fn * schedule) const noexcept
	{
		return { &val_, mutex_.lock_shared(schedule) };
	}

private:
	mutable coro_shared_mutex mutex_;
	T val_;
};
