#pragma once

#include "arc/fwd.hpp"

#include <utility>

template <typename F>
struct arc::extra::on_scope_exit
{
public:
	on_scope_exit() = delete;
	on_scope_exit(const on_scope_exit &) = delete;
	on_scope_exit & operator=(const on_scope_exit &) = delete;
	on_scope_exit(on_scope_exit &&) = delete;
	on_scope_exit & operator=(on_scope_exit &&) = delete;

	on_scope_exit(F && f)
		: f{ std::move(f) }
	{}

	~on_scope_exit() { f(); }

private:
	F f;
};
