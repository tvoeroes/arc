#pragma once

#include "arc/detail/control_block.hpp"
#include "arc/detail/key.hpp"
#include "arc/detail/promise_base.hpp"
#include "arc/util/check.hpp"
#include "arc/util/tracing.hpp"

template <typename T>
struct arc::detail::promise final : arc::detail::promise_base
{
public:
	using handle_type = std::coroutine_handle<promise<T>>;

#if arc_TRACE_INSTRUMENTATION_ENABLE
	promise(const std::source_location & s = std::source_location::current())
	{
		if (const char * n = s.function_name(); n && *n != '\0')
			name = n;
		else
			name = FALLBACK_NAME;
	}
#else
	promise() = default;
#endif

	/** C++ promise API */
	handle_type get_return_object() noexcept
	{
		auto handle = handle_type::from_promise(*this);
		self = handle;
		return handle;
	}

	/** C++ promise API */
	auto final_suspend() const noexcept
	{
		struct final_awaitable
		{
			bool await_ready() const noexcept { return false; }

			bool await_suspend(handle_type awaiter) const noexcept
			{
				awaiter.promise().conditionally_complete();
				return false;
			}

			void await_resume() const noexcept {}
		};

		return final_awaitable{};
	}

	/** C++ promise API */
	void return_value(arc::util::const_removed_t<T> && value) noexcept
	{
		selfHandle->second.construct<T>(std::move(value));
	}

	/** C++ promise API */
	void return_value(const arc::util::const_removed_t<T> & value) noexcept
	{
		selfHandle->second.construct<T>(value);
	}

	/** C++ promise API */
	void return_value(const arc::promise_proxy<T> & value) noexcept
	{
		arc_CHECK_Precondition(selfHandle->second.get_typed<T>() && value.handle == selfHandle);
	}

	/** C++ promise API */
	auto yield_value(T * value)
	{
		arc_CHECK_Precondition(value && value == selfHandle->second.get_typed<T>());

		struct yield_awaitable
		{
			bool await_ready() const noexcept { return false; }

			bool await_suspend(std::coroutine_handle<promise> awaiter) const noexcept
			{
				awaiter.promise().conditionally_complete();
				return false;
			}

			void await_resume() const noexcept {}
		};

		return yield_awaitable{};
	}
};
