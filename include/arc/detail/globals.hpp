#pragma once

#include "arc/util/guard.hpp"

#include <stack>

namespace arc::detail
{
	struct globals;
}

struct arc::detail::globals
{
public:
	globals() = default;

	~globals();

	void add(arc::detail::handle && global);

private:
	arc::util::shared_guard<std::stack<arc::detail::handle>> store;
};
