#include "arc/arc.hpp"

#include <print>
#include <string>

static arc::coro<const std::string * const> make_my_string(
	arc::context & ctx, const std::string & key)
{
	co_return { &key };
}

static arc::coro<const std::string> make_my_hello_world(arc::context & ctx)
{
	arc::result hello = co_await ctx[make_my_string, "Hello, "];
	arc::result world = co_await ctx[make_my_string, "World!"];

	co_return { **hello + **world };
}

int main()
{
	arc::context ctx;
	arc::future future = ctx[make_my_hello_world];
	arc::result result = future.active_wait();

	std::println("{}", *result);
}
