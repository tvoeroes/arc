#pragma once

#include "arc/arc/key_of.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

namespace arc::detail
{
	template <typename F>
	using args_tuple_t = arc::util::remove_tuple_const_reference_t<
		typename arc::detail::reflect_function<std::remove_cvref_t<F>>::argument_tuple>;

	using function_untyped_t = void (*)();

	struct key_impl_base
	{
		virtual bool operator==(const key_impl_base &) const = 0;
		virtual function_untyped_t get_function_untyped() const = 0;
		virtual const void * get_arguments_untyped() const = 0;
		virtual size_t hash_value() const = 0;
		virtual void call(arc::detail::store_entry & storeEntry) const = 0;
		virtual arc::context & get_ctx() const = 0;
		virtual ~key_impl_base() = default;
	};

	template <typename F>
	struct key_impl final : key_impl_base
	{
	public:
		template <typename... Args>
		key_impl(F * function, Args &&... arguments)
			: function_{ function }
			, arguments_{ std::forward<Args>(arguments)... }
		{
			{
				arc::extra::HashAlgorithm hash;

				/**
				 * NOTE: This requires a compiler-specific extension: function-pointer
				 *       (with unspecified value due to the cast) to uintptr_t cast.
				 * NOTE: Hash value for a type may be different on each application
				 *       start because of address space layout randomization.
				 */
				hash_append(hash, reinterpret_cast<uintptr_t>(get_function_untyped()));

				hash_append(hash, arguments_);

				hash_value_ = hash.result();
			}
		}

		bool operator==(const key_impl_base & other) const override
		{
			/**
			 * NOTE: This requires a compiler-specific behavior: comparison of function
			 *       pointers whose value is unspecified due to the cast.
			 */
			if (get_function_untyped() != other.get_function_untyped())
			{
				return false;
			}
			else
			{
				const auto & otherArguments =
					*static_cast<const args_tuple_t<F> *>(other.get_arguments_untyped());
				return arguments_ == otherArguments;
			}
		}

		function_untyped_t get_function_untyped() const override
		{
			return reinterpret_cast<function_untyped_t>(function_);
		}

		const void * get_arguments_untyped() const override { return &arguments_; }

		size_t hash_value() const override { return hash_value_; }

		void call(arc::detail::store_entry & storeEntry) const override;

		arc::context & get_ctx() const override { return std::get<0>(arguments_); }

	private:
		F * function_ = nullptr;
		args_tuple_t<F> arguments_;
		uint64_t hash_value_ = 0;
	};
}

struct arc::detail::key
{
public:
	key() = delete;

	template <typename F, typename... Args>
	key(F * f, arc::context & ctx, Args &&... args)
		: impl_{ std::make_unique<key_impl<F>>(f, ctx, std::forward<Args>(args)...) }
	{}

	arc::context & get_ctx() const { return impl_->get_ctx(); }

	void call(arc::detail::store_entry & storeEntry) const { impl_->call(storeEntry); }

	template <typename T, size_t I, typename F>
	const T & get_key(F * f) const
	{
		if (reinterpret_cast<function_untyped_t>(f) != impl_->get_function_untyped())
			throw std::logic_error("Incorrect function pointer passed to get_key().");

		const auto & args =
			*reinterpret_cast<const args_tuple_t<F> *>(impl_->get_arguments_untyped());

		return std::get<I + 1>(args);
	}

	friend bool operator==(const key & lhs, const key & rhs) { return *lhs.impl_ == *rhs.impl_; }

	size_t hash_value() const { return impl_->hash_value(); }

private:
	std::unique_ptr<key_impl_base> impl_;
};

template <>
struct std::hash<arc::detail::key>
{
	size_t operator()(const arc::detail::key & value) const { return value.hash_value(); }
};
