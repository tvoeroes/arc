#pragma once

#include "arc/extra/on_scope_exit.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

template <typename T, typename Pred>
typename std::vector<T>::iterator arc::extra::insert_sorted(
	std::vector<T> & vec, const T & item, Pred pred)
{
	return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), item);
}

template <typename T>
typename std::vector<T>::iterator arc::extra::insert_sorted(std::vector<T> & vec, const T & item)
{
	return vec.insert(std::upper_bound(vec.begin(), vec.end(), item), item);
}

inline constexpr uint32_t arc::extra::FNV_1a_32(std::string_view str, uint32_t hash)
{
	for (char c : str)
		hash = (hash ^ uint32_t(c)) * uint32_t(16777619u);

	return hash;
}

template <typename Q, typename T>
T arc::extra::queue_pop(Q & queue)
{
	auto popImpl = arc::extra::on_scope_exit{ [&]() { queue.pop(); } };
	return std::move(queue.front());
}
