#include "arc/arc.hpp"

#include <iostream>
#include <string>

struct MyString
{
	const std::string & value;

	static arc::coro<MyString> arc_make(arc::context & ctx, const std::string & key)
	{
		co_return { key };
	}
};

struct MyHelloWorld
{
	std::string value;

	static arc::coro<MyHelloWorld> arc_make(arc::context & ctx)
	{
		arc::result hello = co_await ctx(MyString::arc_make, "Hello, ");
		arc::result world = co_await ctx(MyString::arc_make, "World!");

		co_return { hello->value + world->value };
	}
};

int main()
{
	arc::context ctx;
	arc::future future = ctx(MyHelloWorld::arc_make);
	arc::result result = future.active_wait();

	std::cout << result->value << std::endl;
}
