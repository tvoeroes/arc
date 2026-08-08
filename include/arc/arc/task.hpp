#pragma once

#include "arc/arc/context.hpp"
#include "arc/detail/name_store.hpp"
#include "arc/util/check.hpp"
#include "arc/util/non_copyable_non_movable.hpp"
#include "arc/util/on_scope_exit.hpp"
#include "arc/util/tracing.hpp"

#include <atomic>
#include <coroutine>
#include <exception>
#include <expected>
#include <optional>
#include <source_location>
#include <stop_token>
#include <string_view>
#include <utility>
#include <variant>

#if arc_TRACE_INSTRUMENTATION_ENABLE
	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>
#endif

namespace arc::detail
{
	template <typename T, template <typename> typename TaskType>
	struct task_promise_base;

	template <typename T, template <typename> typename TaskType>
	struct task_promise;
}

namespace arc
{
	template <typename T>
	struct task;

	template <typename T>
	class task_result;

	template <typename T>
	class task_future;
}

template <typename T, template <typename> typename TaskType>
struct arc::detail::task_promise_base
{
public:
	using storage_type = std::conditional_t<
		std::is_void_v<T>, void,
		std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, T>>;
	using coroutine_handle = std::coroutine_handle<task_promise<T, TaskType>>;

	task_promise_base() = default;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	explicit task_promise_base(const std::source_location & s)
		: creation_location_{ s }
	{}
#endif

	arc_NON_COPYABLE_NON_MOVABLE(task_promise_base);

	TaskType<T> get_return_object(this task_promise<T, TaskType> & self)
	{
		return { coroutine_handle::from_promise(self) };
	}

#if arc_TRACE_INSTRUMENTATION_ENABLE
private:
	static void trace_begin_zone(coroutine_handle typed, const std::source_location & s)
	{
		if constexpr (arc_TRACE_CORO)
		{
			std::string_view fn = arc_TRACE_INTERNAL_TRUNCATE_FUNCTION_NAME(s.function_name());
			typed.promise().zone_ctx_ = arc_TRACE_ZONE_BEGIN(fn);
		}
	}

	static void trace_end_zone(coroutine_handle typed)
	{
		if constexpr (arc_TRACE_CORO)
			arc_TRACE_ZONE_END(typed.promise().zone_ctx_);
	}

public:
	struct initial_awaitable
	{
		void * coro_address = nullptr;

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> handle) noexcept
		{
			coro_address = handle.address();
		}

		void await_resume(const std::source_location & s = std::source_location::current()) noexcept
		{
			trace_begin_zone(coroutine_handle::from_address(coro_address), s);
		}
	};

	initial_awaitable initial_suspend() const noexcept { return {}; }
#else
	std::suspend_always initial_suspend() const noexcept { return {}; }
#endif

	struct final_awaitable
	{
		bool await_ready() const noexcept { return false; }

		std::coroutine_handle<> await_suspend(coroutine_handle awaiter) const noexcept
		{
#if arc_TRACE_INSTRUMENTATION_ENABLE
			trace_end_zone(awaiter);
#endif
			if (auto & finalization = awaiter.promise().finalization_; finalization.has_value())
			{
				if (auto * continuation = std::get_if<std::coroutine_handle<>>(&*finalization))
				{
					return *continuation;
				}
				else if (auto * stop_source = std::get_if<std::stop_source>(&*finalization))
				{
					stop_source->request_stop();
				}
				else if (
					auto * fn = std::get_if<arc::function<void(task_result<T>)>>(&*finalization))
				{
					try
					{
						(*fn)(task_result<T>{ awaiter });
					}
					catch (...)
					{
						std::terminate();
					}
				}
			}

			return std::noop_coroutine();
		}

		void await_resume() const noexcept {}
	};

	final_awaitable final_suspend() const noexcept { return {}; }

#if arc_TRACE_INSTRUMENTATION_ENABLE
	template <typename U>
	struct awaitable_wrapper
	{
		U inner;
		void * coro_address = nullptr;

		bool await_ready() noexcept(noexcept(inner.await_ready())) { return inner.await_ready(); }

		template <typename Handle>
		auto await_suspend(Handle handle) noexcept(noexcept(inner.await_suspend(handle)))
		{
			coro_address = handle.address();
			auto typed = coroutine_handle::from_address(coro_address);
			trace_end_zone(typed);
			return inner.await_suspend(handle);
		}

		decltype(auto) await_resume(
			const std::source_location & s =
				std::source_location::current()) noexcept(noexcept(inner.await_resume()))
		{
			if (coro_address)
			{
				trace_begin_zone(coroutine_handle::from_address(coro_address), s);
			}
			return inner.await_resume();
		}
	};

	template <typename U>
	auto await_transform(U && value)
	{
		if constexpr (requires { std::forward<U>(value).operator co_await(); })
		{
			return awaitable_wrapper<
				std::decay_t<decltype(std::forward<U>(value).operator co_await())>>{
				std::forward<U>(value).operator co_await()
			};
		}
		else
		{
			return awaitable_wrapper<U &>{ value };
		}
	}
#endif

	void unhandled_exception() noexcept
	{
		arc_CHECK_Precondition(!result_.has_value());
		result_.emplace(std::unexpect, std::current_exception());
	}

	T get_result()
	{
		arc_CHECK_Precondition(result_.has_value());

		if (result_->has_value())
		{
			if constexpr (std::is_void_v<T>)
				return;
			else if constexpr (std::is_reference_v<T>)
				return *result_->value();
			else
				return std::move(result_->value());
		}
		else
		{
			std::rethrow_exception(result_->error());
		}
	}

	std::optional<std::expected<storage_type, std::exception_ptr>> result_;
	std::optional<std::variant<
		std::coroutine_handle<>, std::stop_source, arc::function<void(task_result<T>)>>>
		finalization_;
#if arc_TRACE_INSTRUMENTATION_ENABLE
	std::source_location creation_location_;
	TracyCZoneCtx zone_ctx_ = {};
#endif
};

template <typename T, template <typename> typename TaskType>
struct arc::detail::task_promise final : public task_promise_base<T, TaskType>
{
public:
	using handle_type = std::coroutine_handle<task_promise>;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	task_promise(const std::source_location & s = std::source_location::current())
		: task_promise_base<T, TaskType>{ s }
	{
		arc::detail::set_zone_info(handle_type::from_promise(*this).address(), "");
	}

	~task_promise() { arc::detail::clear_zone_info(handle_type::from_promise(*this).address()); }
#endif

	void return_value(T && result)
	{
		arc_CHECK_Precondition(!this->result_.has_value());
		if constexpr (std::is_reference_v<T>)
			this->result_.emplace(std::in_place, std::addressof(result));
		else
			this->result_.emplace(std::in_place, std::move(result));
	}

	void return_value(const std::remove_reference_t<T> & result)
		requires(!std::is_reference_v<T>)
	{
		arc_CHECK_Precondition(!this->result_.has_value());
		this->result_.emplace(std::in_place, result);
	}
};

template <template <typename> typename TaskType>
struct arc::detail::task_promise<void, TaskType> final : task_promise_base<void, TaskType>
{
public:
	using handle_type = std::coroutine_handle<task_promise>;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	task_promise(const std::source_location & s = std::source_location::current())
		: task_promise_base<void, TaskType>{ s }
	{
		arc::detail::set_zone_info(handle_type::from_promise(*this).address(), "");
	}

	~task_promise() { arc::detail::clear_zone_info(handle_type::from_promise(*this).address()); }
#endif

	void return_void()
	{
		arc_CHECK_Precondition(!this->result_.has_value());
		this->result_.emplace(std::in_place);
	}
};

template <typename T>
struct arc::task
{
public:
	using promise_type = detail::task_promise<T, task>;

	task() = default;

	task(const task &) = delete;

	task & operator=(const task &) = delete;

	task(task && other) noexcept
		: coroutine_{ std::exchange(other.coroutine_, nullptr) }
	{}

	task & operator=(task && other) noexcept
	{
		if (this != std::addressof(other))
		{
			if (coroutine_)
				coroutine_.destroy();
			coroutine_ = std::exchange(other.coroutine_, nullptr);
		}
		return *this;
	}

	~task()
	{
		if (coroutine_)
			coroutine_.destroy();
	}

	struct awaitable
	{
		bool await_ready() const noexcept { return coroutine_.done(); }

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiter) const noexcept
		{
			arc_CHECK_Precondition(!coroutine_.promise().finalization_.has_value());
			coroutine_.promise().finalization_.emplace(
				std::in_place_type<std::coroutine_handle<>>, awaiter);
			return coroutine_;
		}

		T await_resume() const
		{
			util::on_scope_exit _ = [this] { coroutine_.destroy(); };
			return std::move(coroutine_.promise()).get_result();
		}

		std::coroutine_handle<promise_type> coroutine_;
	};

	awaitable operator co_await() && noexcept
	{
		arc_CHECK_Precondition(coroutine_);
		return { std::exchange(coroutine_, nullptr) };
	}

	task_future<T> sync_launch() &&
	{
		arc_CHECK_Precondition(coroutine_);
		arc_CHECK_Assert(!coroutine_.promise().finalization_.has_value());
		arc_CHECK_Assert(!coroutine_.done());

		auto coroutine = std::exchange(coroutine_, nullptr);
		bool destroy_coroutine = true;
		util::on_scope_exit _ = [&] {
			if (destroy_coroutine)
				coroutine.destroy();
		};
		auto & stop_source = std::get<std::stop_source>(
			coroutine.promise().finalization_.emplace(std::in_place_type<std::stop_source>));
		coroutine.resume();
		destroy_coroutine = false;

		return task_future<T>{ coroutine, &stop_source };
	}

	task_future<T> async_launch(arc::context & ctx, bool onMainThread) &&
	{
		arc_CHECK_Precondition(coroutine_);
		arc_CHECK_Assert(!coroutine_.promise().finalization_.has_value());
		arc_CHECK_Assert(!coroutine_.done());

		auto coroutine = std::exchange(coroutine_, nullptr);
		bool destroy_coroutine = true;
		util::on_scope_exit _ = [&] {
			if (destroy_coroutine)
				coroutine.destroy();
		};
		auto & stop_source = std::get<std::stop_source>(
			coroutine.promise().finalization_.emplace(std::in_place_type<std::stop_source>));
		if (onMainThread)
			ctx.schedule_on_main_thread(std::coroutine_handle<>{ coroutine });
		else
			ctx.schedule_on_worker_thread(std::coroutine_handle<>{ coroutine });
		destroy_coroutine = false;

		return task_future<T>{ coroutine, &stop_source };
	}

	void sync_launch_and_async_wait_and_then(arc::function<void(task_result<T>)> callback) &&
	{
		arc_CHECK_Precondition(coroutine_);
		arc_CHECK_Assert(!coroutine_.promise().finalization_.has_value());
		arc_CHECK_Assert(!coroutine_.done());

		auto coroutine = std::exchange(coroutine_, nullptr);
		bool destroy_coroutine = true;
		util::on_scope_exit _ = [&] {
			if (destroy_coroutine)
				coroutine.destroy();
		};
		coroutine.promise().finalization_.emplace(
			std::in_place_type<arc::function<void(task_result<T>)>>, std::move(callback));
		coroutine.resume();
		destroy_coroutine = false;
	}

	void async_launch_and_async_wait_and_then(
		arc::context & ctx, bool onMainThread, arc::function<void(task_result<T>)> callback) &&
	{
		arc_CHECK_Precondition(coroutine_);
		arc_CHECK_Assert(!coroutine_.promise().finalization_.has_value());
		arc_CHECK_Assert(!coroutine_.done());

		auto coroutine = std::exchange(coroutine_, nullptr);
		bool destroy_coroutine = true;
		util::on_scope_exit _ = [&] {
			if (destroy_coroutine)
				coroutine.destroy();
		};
		coroutine.promise().finalization_.emplace(
			std::in_place_type<arc::function<void(task_result<T>)>>, std::move(callback));
		if (onMainThread)
			ctx.schedule_on_main_thread(std::coroutine_handle<>{ coroutine });
		else
			ctx.schedule_on_worker_thread(std::coroutine_handle<>{ coroutine });
		destroy_coroutine = false;
	}

	/** [[deprecated]] */ std::coroutine_handle<> handle() const noexcept { return coroutine_; }

private:
	std::coroutine_handle<promise_type> coroutine_;

	task(std::coroutine_handle<promise_type> coroutine) noexcept
		: coroutine_{ coroutine }
	{}

	friend struct detail::task_promise_base<T, task>;
};

template <typename T>
class arc::task_result
{
public:
	using promise_type = detail::task_promise<T, task>;

	task_result(const task_result &) = delete;
	task_result & operator=(const task_result &) = delete;

	task_result(task_result && other) noexcept
		: coroutine_{ std::exchange(other.coroutine_, nullptr) }
	{}

	task_result & operator=(task_result && other) noexcept
	{
		if (this != std::addressof(other))
		{
			if (coroutine_)
				coroutine_.destroy();
			coroutine_ = std::exchange(other.coroutine_, nullptr);
		}
		return *this;
	}

	T get()
	{
		arc_CHECK_Precondition(coroutine_);
		auto coroutine = std::exchange(coroutine_, nullptr);
		util::on_scope_exit _ = [coroutine] { coroutine.destroy(); };
		return std::move(coroutine.promise()).get_result();
	}

	~task_result()
	{
		if (coroutine_)
			coroutine_.destroy();
	}

private:
	std::coroutine_handle<promise_type> coroutine_;

	explicit task_result(std::coroutine_handle<promise_type> coroutine) noexcept
		: coroutine_{ coroutine }
	{
		arc_CHECK_Precondition(coroutine_.done());
	}

	friend struct detail::task_promise_base<T, task>;
};

template <typename T>
class arc::task_future
{
public:
	using promise_type = detail::task_promise<T, task>;

	task_future() = default;

	task_future(const task_future &) = delete;

	task_future & operator=(const task_future &) = delete;

	task_future(task_future && other) noexcept
		: coroutine_{ std::exchange(other.coroutine_, nullptr) }
		, stop_source_{ std::exchange(other.stop_source_, nullptr) }
	{}

	task_future & operator=(task_future && other) noexcept
	{
		if (this != std::addressof(other))
		{
			cleanup();
			coroutine_ = std::exchange(other.coroutine_, nullptr);
			stop_source_ = std::exchange(other.stop_source_, nullptr);
		}
		return *this;
	}

	void wait_done() const
	{
		arc_CHECK_Precondition(stop_source_);
		std::atomic_bool done{ false };
		std::stop_callback cb{
			stop_source_->get_token(),
			[&done] {
				done.store(true, std::memory_order_release);
				done.notify_one();
			},
		};
		done.wait(false, std::memory_order_acquire);
	}

	void active_wait_done(arc::context & ctx) const
	{
		arc_CHECK_Precondition(stop_source_);
		ctx.scheduler.assist(stop_source_->get_token());
	}

	bool is_done() const noexcept
	{
		arc_CHECK_Precondition(stop_source_);
		return stop_source_->stop_requested();
	}

	struct co_awaitable
	{
		struct callback_fn
		{
			co_awaitable & parent;
			std::coroutine_handle<> continuation;
			void operator()() const
			{
				if (parent.resumeOnMainThread_)
					parent.ctx_.schedule_on_main_thread(continuation);
				else
					parent.ctx_.schedule_on_worker_thread(continuation);
			}
		};

		std::coroutine_handle<promise_type> coroutine_;
		std::stop_source * stop_source_ = nullptr;
		arc::context & ctx_;
		bool resumeOnMainThread_ = false;
		std::optional<std::stop_callback<callback_fn>> callback_;

		bool await_ready() const noexcept { return stop_source_->stop_requested(); }

		void await_suspend(std::coroutine_handle<> continuation)
		{
			callback_.emplace(stop_source_->get_token(), callback_fn{ *this, continuation });
		}

		T await_resume()
		{
			util::on_scope_exit _ = [this] { coroutine_.destroy(); };
			return std::move(coroutine_.promise()).get_result();
		}
	};

	co_awaitable as_awaitable(arc::context & ctx, bool resumeOnMainThread) &&
	{
		arc_CHECK_Precondition(coroutine_);
		arc_CHECK_Precondition(stop_source_);
		return { std::exchange(coroutine_, nullptr), std::exchange(stop_source_, nullptr), ctx,
				 resumeOnMainThread };
	}

	/** [[deprecated]] */ std::coroutine_handle<> handle() const noexcept { return coroutine_; }

	T active_wait_get(arc::context & ctx)
	{
		arc_CHECK_Precondition(stop_source_);

		active_wait_done(ctx);
		auto coroutine = std::exchange(coroutine_, nullptr);
		stop_source_ = nullptr;
		util::on_scope_exit _ = [coroutine] { coroutine.destroy(); };
		return std::move(coroutine.promise()).get_result();
	}

	T wait_get()
	{
		arc_CHECK_Precondition(stop_source_);

		auto coroutine = coroutine_;
		util::on_scope_exit _ = cleanup();

		return std::move(coroutine.promise()).get_result();
	}

	~task_future() { cleanup(); }

private:
	std::coroutine_handle<promise_type> coroutine_;
	std::stop_source * stop_source_ = nullptr;

	auto cleanup()
	{
		auto coroutine = coroutine_;

		if (coroutine)
		{
			wait_done();
			coroutine_ = nullptr;
			stop_source_ = nullptr;
		}

		return util::on_scope_exit{ [coroutine] {
			if (coroutine)
				coroutine.destroy();
		} };
	}

	task_future(
		std::coroutine_handle<promise_type> coroutine, std::stop_source * stop_source) noexcept
		: coroutine_{ coroutine }
		, stop_source_{ stop_source }
	{
		arc_CHECK_Precondition(bool(coroutine_) == bool(stop_source_));
	}

	friend struct task<T>;
};
