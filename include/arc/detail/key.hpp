#pragma once

#include "arc/arc/key_of.hpp"
#include "arc/arc/tile.hpp"
#include "arc/fwd.hpp"
#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

struct arc::detail::key
{
private:
	using function_storage_type = void (*)();
	using key_variant_type = std::variant<arc::detail::global, int64_t, arc::tile, std::string>;

public:
	using key_type = std::array<key_variant_type, 2>;

	template <typename F>
	static key make(F * f)
	{
		return { reinterpret_cast<function_storage_type>(f),
				 key_type{ arc::detail::global{}, arc::detail::global{} } };
	}

	template <typename F>
	static key make(F * f, arc::key_of_t<F, 0> && key0)
	{
		static constexpr bool is_enum_0 = arc::key_of<F, 0>::is_scoped_enum;

		if constexpr (is_enum_0)
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::to_underlying(key0), arc::detail::global{} } };
		else
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::move(key0), arc::detail::global{} } };
	}

	template <typename F>
	static key make(F * f, arc::key_of_t<F, 0> && key0, arc::key_of_t<F, 1> && key1)
	{
		static constexpr bool is_enum_0 = arc::key_of<F, 0>::is_scoped_enum;
		static constexpr bool is_enum_1 = arc::key_of<F, 1>::is_scoped_enum;

		if constexpr (is_enum_0 && is_enum_1)
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::to_underlying(key0), std::to_underlying(key1) } };
		else if constexpr (is_enum_0)
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::to_underlying(key0), std::move(key1) } };
		else if constexpr (is_enum_1)
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::move(key0), std::to_underlying(key1) } };
		else
			return { reinterpret_cast<function_storage_type>(f),
					 key_type{ std::move(key0), std::move(key1) } };
	}

	/**
	 * NOTE: Undefined behavior if F is wrong!
	 */
	template <typename F>
	const F & get_function() const
	{
		return *reinterpret_cast<F *>(f_);
	}

	template <typename T, size_t I>
	const T & get_key() const
	{
		static_assert(I < decltype(k_){}.size());
		const T * result = nullptr;

		if constexpr (std::is_scoped_enum_v<T>)
		{
			const auto * tmp = std::get_if<std::underlying_type_t<T>>(&k_[I]);
			/* FIXME: Figure out and possibly resolve if this undefined behavior. */
			result = reinterpret_cast<const T *>(tmp);
		}
		else
		{
			result = std::get_if<T>(&k_[I]);
		}

		arc_CHECK_Precondition(result);
		return *result;
	}

	/**
	 * NOTE: This requires a compiler-specific behavior: comparison of function
	 *       pointers whose value is unspecified due to the cast.
	 */
	friend bool operator==(const key &, const key &) = default;

	friend struct std::hash<key>;

private:
	key(function_storage_type f, key_type && k)
		: f_{ f }
		, k_{ std::move(k) }
	{}

	function_storage_type f_ = nullptr;
	key_type k_;
};

template <>
struct std::hash<arc::detail::key>
{
	size_t operator()(const arc::detail::key & key) const
	{
		std::array<size_t, decltype(key.k_){}.size() + 1> h{};

		/**
		 * NOTE: This requires a compiler-specific extension: function-pointer
		 *       (with unspecified value due to the cast) to uintptr_t cast.
		 * NOTE: Hash value for a type may be different on each application
		 *       start because of address space layout randomization.
		 */
		h[0] = size_t(reinterpret_cast<uintptr_t>(key.f_));

		for (size_t i = 0; i < key.k_.size(); i++)
			h[i + 1] = std::hash<arc::detail::key::key_variant_type>{}(key.k_[i]);

		return std::hash<std::string_view>{}(
			{ reinterpret_cast<const char *>(h.data()), sizeof(h[0]) * h.size() });
	}
};
