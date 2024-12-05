#pragma once

#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::tile
{
	int64_t z = 0;
	int64_t y = 0;
	int64_t x = 0;

	inline static constexpr int64_t MAX_Z = 62;

	friend auto operator<=>(const arc::tile & lhs, const arc::tile & rhs) = default;
};

template <>
struct std::hash<arc::tile>
{
	size_t operator()(const arc::tile & tileId) const noexcept
	{
		static_assert(std::has_unique_object_representations_v<arc::tile>);
		return std::hash<std::string_view>{}(
			{ reinterpret_cast<const char *>(&tileId), sizeof(tileId) });
	}
};
