#pragma once

#include "arc/fwd.hpp"

/**
 * Make your struct or class non-movable and non-copyable by privately
 * inheriting from this type:
 * - class MyClass : private non_copyable_non_movable ...
 */
struct arc::extra::non_copyable_non_movable
{
protected:
	non_copyable_non_movable(const non_copyable_non_movable &) = delete;
	non_copyable_non_movable & operator=(const non_copyable_non_movable &) = delete;
	non_copyable_non_movable(non_copyable_non_movable &&) = delete;
	non_copyable_non_movable & operator=(non_copyable_non_movable &&) = delete;

	non_copyable_non_movable() = default;
	~non_copyable_non_movable() = default;
};
