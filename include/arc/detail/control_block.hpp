#pragma once

#include "arc/extra/guard.hpp"
#include "arc/extra/non_copyable_non_movable.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"
#include "arc/util/util.hpp"

/**
 * NOTE: This class uses the scary raw new+delete.
 */
struct arc::detail::control_block : private arc::extra::non_copyable_non_movable
{
public:
	control_block(arc::context & ctx, void (*create)(arc::detail::store_entry &))
		: ctx{ ctx }
		, create{ create }
	{}

	template <typename T, typename... Args>
	T * construct(Args &&... args)
	{
		arc_CHECK_Precondition(!value);
		T * result = new T{ std::forward<Args>(args)... };
		DEBUG_valueIsConst = std::is_const_v<T>;
		value = arc::util::remove_const<T>(result);
		deleter = [](void * value) { delete reinterpret_cast<T *>(value); };
		return result;
	}

	/**
	 * \tparam T If construct<T, ...>() was already called then T must be equal
	 *           to the T argument that construct<T, ...> was called with. Not
	 *           even base class of T is allowed!
	 */
	template <typename T>
	T * get_typed() const
	{
		arc_CHECK_Precondition(value && std::is_const_v<T> == DEBUG_valueIsConst);
		return static_cast<T *>(value);
	}

	void * get_untyped() const
	{
		arc_CHECK_Precondition(value && !DEBUG_valueIsConst);
		return value;
	}

	const void * get_const_untyped() const
	{
		arc_CHECK_Precondition(value && DEBUG_valueIsConst);
		return value;
	}

	~control_block();

private:
	template <typename T>
	friend struct arc::future;

	friend arc::detail::store;
	friend arc::detail::handle;
	friend arc::detail::promise_base;
	friend arc::detail::scheduler;

	template <typename F, size_t keyCount>
	friend void arc::detail::create_shared_task(arc::detail::store_entry & storeEntry);

private:
	void add_reference() noexcept;
	void remove_reference(arc::detail::handle && coroHandle);

	bool is_done() const { return !waiters.ReadOnly()->has_value(); }

	/**
	 * \returns true if the coroutine was scheduled as continuation. False means that the window for
	 *          signaling continuations has passed and that the coroutine can be resumed
	 *          immediately.
	 */
	bool try_add_continuation(std::coroutine_handle<> continuation);

	/**
	 * \returns true if the callback was scheduled. False means that the window
	 *          for signaling continuations has passed and that the callback
	 *          should be handled by the caller of this function instead.
	 */
	bool try_add_callback(std::move_only_function<void()> && callback);

private:
	struct Waiters
	{
		std::vector<std::coroutine_handle<>> continuations;
		std::vector<std::move_only_function<void()>> callbacks;
	};

private:
	arc::context & ctx;

	std::atomic_size_t referenceCount{ 0 };

	arc::detail::promise_base * promiseBase = nullptr;

	void * value = nullptr;
	bool DEBUG_valueIsConst = true;
	std::exception_ptr exception;

	void (*deleter)(void *) = nullptr;
	void (*create)(arc::detail::store_entry &) = nullptr;

	arc::extra::shared_guard<std::optional<Waiters>> waiters{ std::in_place };
};
