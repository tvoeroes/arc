#pragma once

#include "arc/util/on_scope_exit.hpp"
#include "arc/util/util.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace arc::util
{
	/**
	 * Inserts item in to a sorted vec. Inserts after the ties.
	 */
	template <typename Container, typename T, typename Pred>
	typename Container::iterator insert_sorted(Container & vec, T && item, Pred pred)
	{
		return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), std::move(item));
	}

	inline constexpr uint32_t FNV_1a_32(std::string_view str, uint32_t hash = 2166136261u)
	{
		for (char c : str)
			hash = (hash ^ uint32_t(c)) * uint32_t(16777619u);

		return hash;
	}

	struct FNV_1a_64;

	template <typename Hash>
	void hash_append(Hash & hash, const std::monostate & value)
	{
		hash_append(hash, uint8_t(0));
	}

	template <typename Hash>
	void hash_append(Hash & hash, const std::integral auto & value)
	{
		hash.update(&value, sizeof(value));
	}

	template <typename Hash>
	void hash_append(Hash & hash, const std::string & value)
	{
		hash_append(hash, value.size());
		hash.update(value.data(), value.size() * sizeof(value[0]));
	}

	template <typename Hash, typename... Args>
	void hash_append(Hash & hash, const std::variant<Args...> & value)
	{
		hash_append(hash, value.index());
		if (!value.valueless_by_exception())
			std::visit([&hash](const auto & v) { hash_append(hash, v); }, value);
	}

	template <typename Hash>
	void hash_append(Hash & hash, const arc::util::scoped_enum auto & value)
	{
		hash_append(hash, std::to_underlying(value));
	}

	template <typename Hash, typename... Args>
	void hash_append(Hash & hash, const std::tuple<Args...> & value)
	{
		if (std::tuple_size_v<std::tuple<Args...>> == 0)
			hash_append(hash, uint8_t(0));
		else
			std::apply([&hash](const auto &... v) { (hash_append(hash, v), ...); }, value);
	}

	template <typename Hash, typename T>
	void hash_append(Hash & hash, const std::vector<T> & value)
	{
		hash_append(hash, value.size());
		for (const auto & v : value)
			hash_append(hash, v);
	}

	template <typename Q, typename T = typename Q::value_type>
	T queue_pop(Q & queue)
	{
		arc::util::on_scope_exit _ = [&] { queue.pop(); };
		return std::move(queue.front());
	}

	template <typename S, typename T = typename S::value_type>
	T stack_pop(S & stack)
	{
		arc::util::on_scope_exit _ = [&] { stack.pop(); };
		return std::move(stack.top());
	}
}

struct arc::util::FNV_1a_64
{
public:
	using result_type = uint64_t;

	FNV_1a_64() = default;

	void update(const void * data, size_t size) noexcept
	{
		const uint8_t * bytes = static_cast<const uint8_t *>(data);

		for (size_t i = 0; i < size; ++i)
			state = (state ^ uint64_t(bytes[i])) * uint64_t(1099511628211ull);
	}

	uint64_t result() const noexcept { return state; }

private:
	uint64_t state{ 14695981039346656037ull };
};
