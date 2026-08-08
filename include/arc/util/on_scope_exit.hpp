#pragma once

#include "arc/util/non_copyable_non_movable.hpp"

#include <utility>

namespace arc::util
{
	template <typename F>
	struct on_scope_exit;
}

/**
 * Stores f and calls it in the destructor. Recommended usage pattern:
 *
 * arc::util::on_scope_exit _ = [&] { do_something_on_scope_exit(); };
 *
 * Returning an arc::util::on_scope_exit from a function is possible via RVO:
 *
 * auto my_scope()
 * {
 *     my_begin();
 *     return arc::util::on_scope_exit{ [] { my_end(); } };
 * }
 */
template <typename F>
struct arc::util::on_scope_exit
{
public:
	arc_NON_COPYABLE_NON_MOVABLE(on_scope_exit);

	on_scope_exit(F && f)
		: f{ std::move(f) }
	{}

	~on_scope_exit() { f(); }

private:
	F f;
};
