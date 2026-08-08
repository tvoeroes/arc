#pragma once

#include "arc/util/check.hpp"
#include "arc/util/util.hpp"

#include <atomic>
#include <coroutine>
#include <deque>
#include <mutex>
#include <vector>

namespace arc::util
{
	/**
	 * A coroutine-aware shared mutex. Unlike std::shared_mutex, acquiring this mutex from a
	 * coroutine suspends the coroutine rather than blocking the thread when the lock is contended.
	 *
	 * Locks:
	 *  - Exclusive (write): `auto lk = co_await mutex.lock(schedule);`
	 *  - Shared (read):     `auto lk = co_await mutex.lock_shared(schedule);`
	 *
	 * Both return RAII guard objects (exclusive_lock / shared_lock) that release the lock on
	 * destruction.
	 *
	 * The `schedule` parameter passed to lock() / lock_shared() is called with the suspended
	 * coroutine handle when the lock becomes available, instead of resuming it directly. This
	 * allows the caller to control on which thread/executor the coroutine resumes.
	 *
	 * Fairness: shared lock acquisition is blocked while an exclusive waiter is queued, to prevent
	 * writer starvation.
	 */
	class coro_shared_mutex
	{
	public:
		/** Callable invoked to schedule a waiting coroutine handle for resumption. */
		using schedule_fn = arc::function<void(std::coroutine_handle<>) const>;

		coro_shared_mutex() = default;

		coro_shared_mutex(const coro_shared_mutex &) = delete;
		coro_shared_mutex & operator=(const coro_shared_mutex &) = delete;
		coro_shared_mutex(coro_shared_mutex &&) = delete;
		coro_shared_mutex & operator=(coro_shared_mutex &&) = delete;

		struct lock_awaitable;
		struct lock_shared_awaitable;

		/**
		 * RAII guard for an exclusive (Exclusive=true) or shared (Exclusive=false) lock.
		 * Releases the lock on destruction.
		 */
		template <bool Exclusive>
		class basic_lock
		{
		public:
			~basic_lock() noexcept { unlock(); }

			basic_lock(basic_lock && o) noexcept
				: mutex_{ std::exchange(o.mutex_, nullptr) }
			{}

			void unlock() noexcept
			{
				if (mutex_)
				{
					if constexpr (Exclusive)
						mutex_->unlock();
					else
						mutex_->unlock_shared();
					mutex_ = nullptr;
				}
			}

			basic_lock(const basic_lock &) = delete;
			basic_lock & operator=(basic_lock &&) = delete;
			basic_lock & operator=(const basic_lock &) = delete;

		private:
			explicit basic_lock(coro_shared_mutex * m) noexcept
				: mutex_{ m }
			{}

			coro_shared_mutex * mutex_ = nullptr;

			friend struct lock_awaitable;
			friend struct lock_shared_awaitable;
		};

		using exclusive_lock = basic_lock<true>;
		using shared_lock = basic_lock<false>;

		struct lock_awaitable
		{
			bool await_ready() noexcept { return mutex_->try_lock_exclusive_nowait(); }

			bool await_suspend(std::coroutine_handle<> h) noexcept
			{
				return !mutex_->try_lock_exclusive_or_queue(h, schedule_);
			}

			exclusive_lock await_resume() noexcept { return exclusive_lock{ mutex_ }; }

			coro_shared_mutex * mutex_;
			const schedule_fn * schedule_;
		};

		struct lock_shared_awaitable
		{
			bool await_ready() noexcept { return mutex_->try_lock_shared_nowait(); }

			bool await_suspend(std::coroutine_handle<> h) noexcept
			{
				return !mutex_->try_lock_shared_or_queue(h, schedule_);
			}

			shared_lock await_resume() noexcept { return shared_lock{ mutex_ }; }

			coro_shared_mutex * mutex_;
			const schedule_fn * schedule_;
		};

		lock_awaitable lock(const schedule_fn * schedule) noexcept { return { this, schedule }; }

		lock_shared_awaitable lock_shared(const schedule_fn * schedule) noexcept
		{
			return { this, schedule };
		}

	private:
		void unlock() noexcept
		{
			std::vector<arc::function<void()>> to_wake;
			{
				std::lock_guard guard{ m_ };
				arc_CHECK_Precondition(state_ == -1);
				state_ = 0;
				to_wake = collect_wakers_locked();
			}
			for (auto & fn : to_wake)
				fn();
		}

		void unlock_shared() noexcept
		{
			std::vector<arc::function<void()>> to_wake;
			{
				std::lock_guard guard{ m_ };
				arc_CHECK_Precondition(state_ > 0);
				--state_;
				if (state_ == 0)
					to_wake = collect_wakers_locked();
			}
			for (auto & fn : to_wake)
				fn();
		}

		/**
		 * Attempts to acquire an exclusive lock without queuing. Returns true if the lock was
		 * acquired, false if it is currently held (no side effects in that case).
		 */
		bool try_lock_exclusive_nowait() noexcept
		{
			std::lock_guard guard{ m_ };
			if (state_ == 0 && waiters_.empty())
			{
				state_ = -1;
				return true;
			}
			return false;
		}
		/**
		 * Attempts to acquire an exclusive lock. If unavailable, queues the coroutine handle with
		 * the given schedule function and returns false (caller should suspend). Returns true if
		 * the lock was acquired immediately.
		 */
		bool try_lock_exclusive_or_queue(
			std::coroutine_handle<> h, const schedule_fn * schedule) noexcept
		{
			std::lock_guard guard{ m_ };
			if (state_ == 0 && waiters_.empty())
			{
				state_ = -1;
				return true;
			}
			waiters_.push_back(
				{
					[schedule, h]() { (*schedule)(h); },
					true,
				});
			return false;
		}

		/**
		 * Attempts to acquire a shared lock without queuing. Returns true if the lock was acquired.
		 */
		bool try_lock_shared_nowait() noexcept
		{
			std::lock_guard guard{ m_ };
			if (state_ >= 0 && !has_exclusive_waiter_locked())
			{
				++state_;
				return true;
			}
			return false;
		}
		/**
		 * Attempts to acquire a shared lock. If unavailable, queues the coroutine handle with the
		 * given schedule function and returns false (caller should suspend). Returns true if the
		 * lock was acquired immediately.
		 */
		bool try_lock_shared_or_queue(
			std::coroutine_handle<> h, const schedule_fn * schedule) noexcept
		{
			std::lock_guard guard{ m_ };
			if (state_ >= 0 && !has_exclusive_waiter_locked())
			{
				++state_;
				return true;
			}
			waiters_.push_back(
				{
					[schedule, h]() { (*schedule)(h); },
					false,
				});
			return false;
		}

		struct waiter_entry
		{
			/** called outside m_ to resume or unblock the waiter */
			arc::function<void()> wake;
			bool exclusive;
		};

		bool has_exclusive_waiter_locked() const noexcept
		{
			for (const auto & w : waiters_)
				if (w.exclusive)
					return true;
			return false;
		}

		std::vector<arc::function<void()>> collect_wakers_locked()
		{
			std::vector<arc::function<void()>> result;
			if (waiters_.empty())
				return result;

			if (waiters_.front().exclusive)
			{
				state_ = -1;
				result.push_back(std::move(waiters_.front().wake));
				waiters_.pop_front();
			}
			else
			{
				while (!waiters_.empty() && !waiters_.front().exclusive)
				{
					++state_;
					result.push_back(std::move(waiters_.front().wake));
					waiters_.pop_front();
				}
			}
			return result;
		}

		std::mutex m_;
		/** 0 = unlocked, -1 = exclusively locked, N > 0 = N shared locks held */
		int64_t state_ = 0;
		std::deque<waiter_entry> waiters_;
	};

}
