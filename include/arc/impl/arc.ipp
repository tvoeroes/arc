#pragma once

#include "arc/util/tracing.hpp"

template <typename F>
arc::future<arc::result_of_t<F>> arc::context::operator()(F * f)
{
	static_assert(arc::key_count_of_v<F> == 0);
	return store.retrieve_reference(*this, f, arc::detail::key::make(f));
}

template <typename F>
inline arc::future<arc::result_of_t<F>> arc::context::operator()(F * f, arc::key_of_t<F, 0> key0)
{
	static_assert(arc::key_count_of_v<F> == 1);
	return store.retrieve_reference(*this, f, arc::detail::key::make(f, std::move(key0)));
}

template <typename F>
arc::future<arc::result_of_t<F>> arc::context::operator()(
	F * f, arc::key_of_t<F, 0> key0, arc::key_of_t<F, 1> key1)
{
	static_assert(arc::key_count_of_v<F> == 2);
	return store.retrieve_reference(
		*this, f, arc::detail::key::make(f, std::move(key0), std::move(key1)));
}

inline auto arc::context::schedule_on_worker_thread()
{
	return scheduler.schedule(std::nullopt, false);
}

inline auto arc::context::schedule_on_worker_thread_after(arc::time_point timePoint)
{
	return scheduler.schedule(timePoint, false);
}

inline auto arc::context::schedule_on_main_thread()
{
	return scheduler.schedule(std::nullopt, true);
}

inline auto arc::context::schedule_on_main_thread_after(arc::time_point timePoint)
{
	return scheduler.schedule(timePoint, true);
}

template <typename T>
inline auto arc::get_self_reference()
{
	struct awaitable
	{
		bool await_ready() const noexcept { return false; }

		bool await_suspend(std::coroutine_handle<arc::detail::promise<T>> awaiter) noexcept
		{
			handle = awaiter.promise().handle();
			return false;
		}

		arc::future<T> await_resume() noexcept { return arc::future<T>{ std::move(handle) }; }

		arc::detail::handle handle;
	};

	return awaitable{};
}

template <typename T>
inline auto arc::get_promise_proxy()
{
	struct awaitable
	{
		bool await_ready() const noexcept { return false; }

		bool await_suspend(std::coroutine_handle<arc::detail::promise<T>> awaiter) noexcept
		{
			handle = awaiter.promise().handle();
			return false;
		}

		arc::promise_proxy<T> await_resume() noexcept { return { std::move(handle) }; }

		arc::detail::handle handle;
	};

	return awaitable{};
}

template <typename T>
struct arc::future<T>::impl
{
	static arc::result<T> get_result(arc::future<T> & self)
	{
		arc_CHECK_Precondition(self.handle);

		if (self.handle->second.exception)
		{
			arc_CHECK_Assert(!self.handle->second.value);
			std::rethrow_exception(self.handle->second.exception);
		}

		util::const_matching_void_t<T> * voidPtr = nullptr;
		if constexpr (std::is_const_v<T>)
			voidPtr = self.handle->second.get_const_untyped();
		else
			voidPtr = self.handle->second.get_untyped();
		arc_CHECK_Assert(voidPtr);
		T * ptr = self.resolve ? self.resolve(voidPtr) : static_cast<T *>(voidPtr);
		self.resolve = nullptr;
		return { ptr, std::move(self.handle) };
	}

	template <typename U>
	static resolve_type copy_resolve_inheritance(const future<U> & other)
	{
		static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U> || std::is_void_v<T>);

		if constexpr (std::is_same_v<T, U>)
		{
			return other.resolve;
		}
		else
		{
			arc_CHECK_Precondition(!other.resolve);

			return resolve_inheritance<U, std::is_const_v<U>>;
		}
	}

	template <typename U>
	static resolve_type move_resolve_inheritance(future<U> & other)
	{
		resolve_type result = copy_resolve_inheritance(other);
		other.resolve = nullptr;
		return result;
	}

	template <auto M, typename U>
	static resolve_type copy_resolve_member(const future<U> & other)
	{
		arc_CHECK_Precondition(!other.resolve);

		using result_info = arc::detail::reflect_pointer_to_member<M>;

		using class_type = std::conditional_t<
			std::is_const_v<U>, const typename result_info::class_type,
			typename result_info::class_type>;

		using member_type = std::conditional_t<
			std::is_const_v<U>, const typename result_info::member_type,
			typename result_info::member_type>;

		static_assert(M);
		static_assert(std::is_same_v<U, class_type>);

		return resolve_member<
			member_type, class_type, result_info::member_pointer, std::is_const_v<U>>;
	}

	template <auto M, typename U>
	static resolve_type move_resolve_member(future<U> & other)
	{
		resolve_type result = copy_resolve_member<M>(other);
		other.resolve = nullptr;
		return result;
	}

	template <typename U, bool C>
	static T * resolve_inheritance(std::conditional_t<C, const void, void> * p)
	{
		return static_cast<T *>(static_cast<U *>(p));
	}

	template <typename U, typename V, U V::* M, bool C>
	static U * resolve_member(std::conditional_t<C, const void, void> * p)
	{
		return p ? &(static_cast<V *>(p)->*M) : nullptr;
	}

	template <typename U>
	static bool is_same_object(const arc::future<T> & self, const future<U> & other)
	{
		if constexpr (std::is_same_v<T, U>)
			return &self == std::addressof(other);
		else
			return false;
	}
};

template <typename T>
inline arc::future<T>::future(future && other)
	: handle{ std::exchange(other.handle, nullptr) }
	, resolve{ impl::move_resolve_inheritance(other) }
{}

template <typename T>
inline arc::future<T>::future(const future & other)
	: handle{ other.handle }
	, resolve{ impl::copy_resolve_inheritance(other) }
{}

template <typename T>
template <typename U>
inline arc::future<T>::future(future<U> && other)
	: handle{ std::exchange(other.handle, nullptr) }
	, resolve{ impl::move_resolve_inheritance(other) }
{}

template <typename T>
template <typename U>
inline arc::future<T>::future(const future<U> & other)
	: handle{ other.handle }
	, resolve{ impl::copy_resolve_inheritance(other) }
{}

template <typename T>
template <auto M, typename U>
inline arc::future<T>::future(arc::util::value_tag<M>, future<U> && other)
	: handle{ std::exchange(other.handle, nullptr) }
	, resolve{ impl::template move_resolve_member<M>(other) }
{}

template <typename T>
template <auto M, typename U>
inline arc::future<T>::future(arc::util::value_tag<M>, const future<U> & other)
	: handle{ other.handle }
	, resolve{ impl::template copy_resolve_member<M>(other) }
{}

template <typename T>
inline arc::future<T> & arc::future<T>::operator=(future && other)
{
	if (!impl::is_same_object(*this, other))
	{
		handle = std::exchange(other.handle, nullptr);
		resolve = impl::move_resolve_inheritance(other);
	}

	return *this;
}

template <typename T>
inline arc::future<T> & arc::future<T>::operator=(const future & other)
{
	if (!impl::is_same_object(*this, other))
	{
		handle = other.handle;
		resolve = impl::copy_resolve_inheritance(other);
	}

	return *this;
}

template <typename T>
template <typename U>
inline arc::future<T> & arc::future<T>::operator=(future<U> && other)
{
	if (!impl::is_same_object(*this, other))
	{
		handle = std::exchange(other.handle, nullptr);
		resolve = impl::move_resolve_inheritance(other);
	}

	return *this;
}

template <typename T>
template <typename U>
inline arc::future<T> & arc::future<T>::operator=(const future<U> & other)
{
	if (!impl::is_same_object(*this, other))
	{
		handle = other.handle;
		resolve = impl::copy_resolve_inheritance(other);
	}

	return *this;
}

template <typename T>
inline arc::future<T> & arc::future<T>::operator=(std::nullptr_t)
{
	handle = nullptr;
	resolve = nullptr;
	return *this;
}

template <typename T>
inline arc::future<T>::operator bool() const noexcept
{
	return !!handle;
}

template <typename T>
inline void arc::future<T>::async_wait_and_then(std::move_only_function<void()> && callback) const
{
	arc_CHECK_Precondition(handle);

	if (bool added = handle->second.try_add_callback(std::move(callback)); !added)
		callback();
}

template <typename T>
inline void arc::future<T>::async_wait_and_then(
	std::move_only_function<void(arc::result<T>)> && callback) const
{
	async_wait_and_then([c = std::move(callback), a = *this]() mutable { c(impl::get_result(a)); });
}

template <typename T>
inline arc::result<T> arc::future<T>::active_wait()
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

	arc_CHECK_Precondition(handle);

	std::stop_source stopSource;

	async_wait_and_then([&stopSource] { stopSource.request_stop(); });

	handle->second.ctx.scheduler.assist(stopSource.get_token());

	return impl::get_result(*this);
}

template <typename T>
inline arc::result<T> arc::future<T>::try_wait()
{
	arc_CHECK_Precondition(handle);

	if (handle->second.is_done())
		return impl::get_result(*this);
	else
		return {};
}

template <typename T>
inline auto arc::future<T>::operator co_await()
{
	struct awaitable
	{
		bool await_ready() const noexcept { return false; }

		bool await_suspend(std::coroutine_handle<> awaiter)
		{
			arc_CHECK_Precondition(self.handle);

			return self.handle->second.try_add_continuation(awaiter);
		}

		arc::result<T> await_resume() { return impl::get_result(self); }

		arc::future<T> & self;
	};

	return awaitable{ *this };
}

template <typename T>
inline arc::future<T>::future() = default;

template <typename T>
inline arc::future<T>::future(std::nullptr_t)
	: future{}
{}

template <typename T>
inline arc::future<T>::future(arc::detail::handle && handle)
	: handle{ std::move(handle) }
{}

template <typename T>
struct arc::result<T>::impl
{
	template <typename U>
	static bool is_same_object(const result<T> & self, const result<U> & other)
	{
		if constexpr (std::is_same_v<T, U>)
			return &self == std::addressof(other);
		else
			return false;
	}

	template <typename U>
	static T * up_cast(U * other)
	{
		return static_cast<T *>(other);
	}
};

template <typename T>
inline arc::result<T>::result(result && other)
	: value{ impl::up_cast(std::exchange(other.value, nullptr)) }
	, handle{ std::exchange(other.handle, nullptr) }
{}

template <typename T>
inline arc::result<T>::result(const result & other)
	: value{ impl::up_cast(other.value) }
	, handle{ other.handle }
{}

template <typename T>
template <typename U>
inline arc::result<T>::result(result<U> && other)
	: value{ impl::up_cast(std::exchange(other.value, nullptr)) }
	, handle{ std::exchange(other.handle, nullptr) }
{}

template <typename T>
template <typename U>
inline arc::result<T>::result(const result<U> & other)
	: value{ impl::up_cast(other.value) }
	, handle{ other.handle }
{}

template <typename T>
template <typename U>
inline arc::result<T>::result(T * value, result<U> && other)
	: value{ value }
	, handle{ std::exchange(other.handle, nullptr) }
{
	arc_CHECK_Precondition(!!this->value == !!this->handle);
	other.value = nullptr;
}

template <typename T>
template <typename U>
inline arc::result<T>::result(T * value, const result<U> & other)
	: value{ value }
	, handle{ other.handle }
{
	arc_CHECK_Precondition(!!this->value == !!this->handle);
}

template <typename T>
inline arc::result<T> & arc::result<T>::operator=(result && other)
{
	if (!impl::is_same_object(*this, other))
	{
		value = impl::up_cast(std::exchange(other.value, nullptr));
		handle = std::exchange(other.handle, nullptr);
	}

	return *this;
}

template <typename T>
inline arc::result<T> & arc::result<T>::operator=(const result & other)
{
	if (!impl::is_same_object(*this, other))
	{
		value = impl::up_cast(other.value);
		handle = other.handle;
	}

	return *this;
}

template <typename T>
template <typename U>
inline arc::result<T> & arc::result<T>::operator=(result<U> && other)
{
	if (!impl::is_same_object(*this, other))
	{
		value = impl::up_cast(std::exchange(other.value, nullptr));
		handle = std::exchange(other.handle, nullptr);
	}

	return *this;
}

template <typename T>
template <typename U>
inline arc::result<T> & arc::result<T>::operator=(const result<U> & other)
{
	if (!impl::is_same_object(*this, other))
	{
		value = impl::up_cast(other.value);
		handle = other.handle;
	}

	return *this;
}

template <typename T>
inline arc::result<T> & arc::result<T>::operator=(std::nullptr_t)
{
	value = nullptr;
	handle = nullptr;
	return *this;
}

template <typename T>
arc::detail::handle arc::result<T>::extract_handle() &&
{
	value = nullptr;
	return std::move(handle);
}

template <typename T>
arc::detail::handle arc::future<T>::extract_handle() &&
{
	resolve = nullptr;
	return std::move(handle);
}

template <typename T>
inline arc::result<T>::result(T * value, arc::detail::handle && handle) noexcept
	: value{ value }
	, handle{ std::move(handle) }
{
	arc_CHECK_Precondition(!!this->value == !!this->handle);
}

template <typename T>
inline T * arc::result<T>::get() const noexcept
{
	return value;
}

template <typename T>
inline T * arc::result<T>::operator->() const noexcept
{
	arc_CHECK_Precondition(*this);
	return get();
}

template <typename T>
template <typename T2, std::enable_if_t<!std::is_void_v<T2>, int>>
inline T2 & arc::result<T>::operator*() const noexcept
{
	arc_CHECK_Precondition(*this);
	return *get();
}

template <typename T>
inline arc::result<T>::operator bool() const noexcept
{
	return value;
}

template <typename T>
template <typename K, size_t I>
inline const K & arc::result<T>::get_key() const
{
	arc_CHECK_Precondition(handle);
	return handle->first.get_key<K, I>();
}

template <typename T>
inline arc::result<T>::result() = default;

template <typename T>
inline arc::result<T>::result(std::nullptr_t)
	: result{}
{}

template <typename F, size_t keyCount>
void arc::detail::create_shared_task(arc::detail::store_entry & storeEntry)
{
	const arc::detail::key & key = storeEntry.first;
	arc::detail::control_block & controlBlock = storeEntry.second;

	std::coroutine_handle<arc::detail::promise<arc::result_of_t<F>>> handle = [&] {
		static_assert(keyCount >= 0 && keyCount <= 2);

		static constexpr bool isAlreadyCoro = arc::detail::is_coro_v<
			typename arc::detail::reflect_function<std::remove_cvref_t<F>>::return_type>;

		if constexpr (keyCount == 0)
		{
			if constexpr (isAlreadyCoro)
			{
				return key.get_function<F>()(controlBlock.ctx);
			}
			else
			{
				auto wrapper = [](F f, arc::context & ctx) -> arc::coro<arc::result_of_t<F>> {
					/** */
					co_return f(ctx);
				};
				return wrapper(key.get_function<F>(), controlBlock.ctx);
			}
		}
		else if constexpr (keyCount == 1)
		{
			using key_type_0 = arc::key_of_t<F, 0>;

			if constexpr (isAlreadyCoro)
			{
				return key.get_function<F>()(controlBlock.ctx, key.get_key<key_type_0, 0>());
			}
			else
			{
				auto wrapper = [](F f, arc::context & ctx,
								  const key_type_0 & key0) -> arc::coro<arc::result_of_t<F>> {
					/** */
					co_return f(ctx, key0);
				};
				return wrapper(key.get_function<F>(), controlBlock.ctx, key.get_key<key_type_0>());
			}
		}
		else if constexpr (keyCount == 2)
		{
			using key_type_0 = arc::key_of_t<F, 0>;
			using key_type_1 = arc::key_of_t<F, 1>;

			if constexpr (isAlreadyCoro)
			{
				return key.get_function<F>()(
					controlBlock.ctx, key.get_key<key_type_0, 0>(), key.get_key<key_type_1, 1>());
			}
			else
			{
				auto wrapper = [](F f, arc::context & ctx, const key_type_0 & key0,
								  const key_type_1 & key1) -> arc::coro<arc::result_of_t<F>> {
					/** */
					co_return f(ctx, key0, key1);
				};
				return wrapper(
					key.get_function<F>(), controlBlock.ctx, key.get_key<key_type_0>(),
					key.get_key<key_type_1>());
			}
		}
	}();

	arc_CHECK_Require(!controlBlock.exception && !controlBlock.value);

	arc_CHECK_Require(!controlBlock.promiseBase);

	controlBlock.promiseBase = &handle.promise();

	controlBlock.promiseBase->set_self_handle(arc::detail::handle{ &storeEntry });

#ifdef arc_TRACE_INSTRUMENTATION_ENABLE
	controlBlock.ctx.names.set_name(handle, controlBlock.promiseBase->name);
#endif

	controlBlock.ctx.schedule_on_worker_thread(controlBlock.promiseBase->self);
}

template <typename F>
arc::future<arc::result_of_t<F>> arc::detail::store::retrieve_reference(
	arc::context & ctx, F * f, arc::detail::key && key)
{
	arc_TRACE_EVENT_SCOPED(arc_TRACE_CORO);

	auto dataHandle = data.ReadAndWrite();

	auto insertion =
		dataHandle->store.try_emplace(std::move(key), ctx, arc::detail::create_shared_task<F>);
	auto it = insertion.first;

	if (insertion.second)
		it->second.create(*it);

	return arc::future<arc::result_of_t<F>>{ arc::detail::handle{ &*it } };
}

template <typename T>
void arc::context::set_caching_policy_global(arc::future<T> global)
{
	globals.add(std::move(global).extract_handle());
}

template <typename T>
void arc::context::set_caching_policy_global(arc::result<T> global)
{
	globals.add(std::move(global).extract_handle());
}
