#include "arc/arc.hpp"

#include "arc/extra/non_copyable_non_movable.hpp"
#include "testing_macros.hpp"

#include <array>
#include <optional>
#include <stdexcept>
#include <string>

struct CoroHelloWorld
{
	std::string message;

	static arc::coro<CoroHelloWorld> arc_make(arc::context & ctx) { co_return { "Hello, World!" }; }
};

TEST_CASE("Coro Hello World", "[Coro]")
{
	arc::context ctx;
	arc::result result = ctx(CoroHelloWorld::arc_make).active_wait();
	CHECK(result->message == "Hello, World!");
}

struct CoroHelloMessage
{
	const std::string & message;

	/**
	 * This function is used as the constructor for the result. It must have the
	 * signature pattern as shown below. The second and third arguments are
	 * either to present (global) or one of possible key types.
	 */
	static arc::coro<CoroHelloMessage> arc_make(arc::context & ctx, const std::string & key)
	{
		/**
		 * The key object remains valid for the lifetime of the result.
		 */
		co_return { key };
	}
};

struct CoroHelloTile
{
	const arc::tile & tileId;

	static arc::coro<CoroHelloTile> arc_make(arc::context & ctx, const arc::tile & key)
	{
		co_return { key };
	}
};

struct CoroBaseResult
{
	int value = 0;
	int extra1 = 10;
	int extra2 = 20;
	int extra3 = 30;
};

struct CoroDerivedResult1 : CoroBaseResult
{
	static arc::coro<CoroDerivedResult1> arc_make(arc::context & ctx) { co_return { 1 }; }
};

struct CoroDerivedResult2 : CoroBaseResult
{
	int z = 33;

	static arc::coro<CoroDerivedResult2> arc_make(arc::context & ctx) { co_return { 2 }; }
};

TEST_CASE("Coro Synchronous Wait", "[Coro]")
{
	SECTION("using a key")
	{
		arc::context ctx;
		arc::result result = ctx(CoroHelloMessage::arc_make, "Hello, Message!").active_wait();
		CHECK(result->message == "Hello, Message!");
	}

	SECTION("use a thread pool")
	{
		arc::context ctx{ arc::options::two_threads() };
		arc::result result = ctx(CoroHelloMessage::arc_make, "Hello, Threads!").active_wait();
		CHECK(result->message == "Hello, Threads!");
	}

	SECTION("synchronously access multiple results")
	{
		arc::context ctx;

		arc::result result = ctx(CoroHelloTile::arc_make, { 10, 326, 510 }).active_wait();

		/** the result is available after Wait returns */
		{
			REQUIRE(result);
			CHECK(result->tileId == arc::tile{ 10, 326, 510 });
		}

		/** release result references */
		{
			result = nullptr;
			CHECK_FALSE(result);
		}

		arc::future future = ctx(CoroHelloTile::arc_make, arc::tile{ 5, 13, 4 });

		/** future wait function */
		result = future.active_wait();
		CHECK(result->tileId == arc::tile{ 5, 13, 4 });
	}

	SECTION("poll result")
	{
		arc::options options;

		/**
		 * requesting one additional thread to be spawned to assist because
		 * neither arc::context::Get() nor arc::future::try_wait() do
		 * participate in computation of the result
		 */
		options.workerThreadCount = 1;

		arc::context ctx{ options };

		arc::future future = ctx(CoroHelloMessage::arc_make, "Polling");

		arc::result<CoroHelloMessage> result;

		do
		{
			std::this_thread::yield(); /** this yield is optional */
			result = future.try_wait();
		} while (!result);

		CHECK(result);

		arc::future future2 = ctx(CoroHelloMessage::arc_make, "Polling");

		arc::result<CoroHelloMessage> result2 = future2.try_wait();

		/**
		 * the result already exists therefore arc::future::try_wait() returns
		 * the result on the first call
		 */
		CHECK(result2);

		/** try_wait() extracts the handle on success */
		CHECK(!future2);

		CHECK(result.get() == result2.get());
	}

	SECTION("abandon a future")
	{
		arc::context ctx;
		/**
		 * a future can be abandoned without awaiting it but the result will
		 * still be calculated and destructed
		 */
		arc::future result = ctx(CoroHelloMessage::arc_make, "Unused :(");

		/** future can be checked for whether it can be awaited on */
		CHECK(result);
	}

	SECTION("casting future to base class and polymorphic containers")
	{
		arc::context ctx;

		std::array<arc::future<CoroBaseResult>, 2> futures;
		std::array<arc::result<CoroBaseResult>, 2> results;

		/** up-casting */
		futures[0] = ctx(CoroDerivedResult1::arc_make);
		futures[1] = ctx(CoroDerivedResult2::arc_make);

		/**
		 *  WARNING: there is currently a limitation on how many times an
		 *           up-cast is possible on a future. A future can not be
		 *           up-casted if it has been created as a result of an up-cast.
		 *           This is a limitation of the current implementation. Also
		 *           note that a result does not have a similar restriction
		 */

		/** operation on polymorphic container */
		for (size_t i = 0; i < 2; i++)
			results[i] = futures[i].active_wait();

		CHECK(results[0]->value == 1);
		CHECK(results[1]->value == 2);
	}

	SECTION("RAII with future and result")
	{
		arc::context ctx;

		arc::future future1 = ctx(CoroDerivedResult1::arc_make);

		/** copy construct */
		arc::future future2 = future1;

		/** move construct */
		arc::future future3{ std::move(future1) };

		arc::future<CoroDerivedResult1> future4;
		arc::future<CoroDerivedResult1> future5;

		arc::future<CoroBaseResult> future6;
		arc::future<CoroBaseResult> future7;
		arc::future<CoroBaseResult> future8;

		/** copy assign */
		future4 = future2;

		/** move assign */
		future5 = std::move(future2);

		/** up-cast copy assign */
		future6 = future4;

		/** up-cast move assign */
		future7 = std::move(future4);
		future8 = std::move(future7);

		CHECK(!future7);
		future7 = future8;
		CHECK(future7);

		arc::future future9 = ctx(CoroDerivedResult2::arc_make);

		/** member aliasing */
		arc::future<int> future10{ arc::util::value_tag<&CoroDerivedResult2::z>{}, future9 };
		CHECK(future9);
		arc::future<int> future11{ arc::util::value_tag<&CoroDerivedResult2::z>{},
								   std::move(future9) };
		CHECK(!future9);

		arc::result result1 = future10.active_wait();
		arc::result result2 = future11.active_wait();

		CHECK(*result1 == 33);
		CHECK(*result2 == 33);
		CHECK(result1.get() == result2.get());

		/** result is a std::shared_ptr-like pointer container */
		arc::result result3 = future3.active_wait();
		arc::result result4 = future6.active_wait();
		arc::result result5 = future7.active_wait();

		/** active_wait() extracts the handle */
		CHECK(!future3);
		CHECK(!future6);
		CHECK(!future7);

		CHECK(result3->value == 1);
		CHECK(result4->value == 1);
		CHECK(result5->value == 1);

		/** it is all the same object */
		CHECK(&result3->value == &result4->value);
		CHECK(&result3->value == &result5->value);

		/** copy construct */
		arc::result result6 = result3;

		/** move construct */
		arc::result result7 = std::move(result3);

		/** reference was moved out, now it holds no result */
		CHECK(!result3);

		arc::result<CoroBaseResult> result8;
		arc::result<CoroBaseResult> result9;

		/** default constructed result holds no value */
		CHECK(!result8);
		CHECK(!result9);

		/** copy assign */
		result8 = result4;

		CHECK(result8.get() == result4.get());

		/** move assign */
		result9 = std::move(result5);

		/** reference was moved out, now it holds no result */
		CHECK(!result5);

		/** aliasing copy construct */
		arc::result<int> result12{ &result9->value, result9 };

		/** aliasing move construct */
		arc::result<int> result13{ &result9->value, std::move(result9) };

		/** reference was moved out, now it holds no result */
		CHECK(!result9);

		CHECK(*result12 == 1);
		CHECK(*result13 == 1);

		/** result can be reset by assigning nullptr to it */
		result12 = nullptr;

		CHECK(!result12);

		/** type-erased future */
		arc::future future12 = ctx(CoroDerivedResult1::arc_make);
		arc::future<CoroDerivedResult1> future13 = future12;
		arc::future<CoroDerivedResult1> future14 = future12;
		arc::future<void> future15 = future12;
		arc::future<void> future16{ std::move(future12) };
		arc::future<void> future17 = future15;
		arc::future<void> future18 = std::move(future15);

		CHECK(!future12);
		CHECK(!future15);

		arc::result<CoroDerivedResult1> result14 = future13.active_wait();
		arc::result<void> result15 = future14.active_wait();
		arc::result<void> result16 = future16.active_wait();
		arc::result<void> result17{ future17.active_wait() };
		arc::result<void> result18 = future18.active_wait();

		CHECK(static_cast<void *>(result14.get()) == result15.get());
		CHECK(result15.get() == result16.get());
		CHECK(result15.get() == result17.get());
		CHECK(result15.get() == result18.get());

		/** lets abandon this future */
		CHECK(future5);
	}
}

void throw_on_bad_input_for_fibonacci(int64_t n)
{
	if (n < 0)
		throw std::domain_error("Fibonacci numbers are defined for n >= 0.");

	if (n > 92)
		throw std::overflow_error(
			"Fibonacci numbers n > 92 can not be represented by a 64bit signed integer.");
}

/**
 * Time Complexity: Between O(N) and O(2^N). Depends on result caching settings.
 */
struct CoroRecursiveFibonacci
{
	int64_t value = 0;

	static arc::coro<CoroRecursiveFibonacci> arc_make(arc::context & ctx, const int64_t & n)
	{
		throw_on_bad_input_for_fibonacci(n);

		if (n < 2)
			co_return { n };

		arc::result a = co_await ctx(CoroRecursiveFibonacci::arc_make, n - 1);
		arc::result b = co_await ctx(CoroRecursiveFibonacci::arc_make, n - 2);

		co_return { a->value + b->value };
	}
};

/**
 * Time Complexity: O(N).
 */
struct CoroRecursiveCachedFibonacci
{
	int64_t value = 0;

	arc::result<CoroRecursiveCachedFibonacci> a;
	arc::result<CoroRecursiveCachedFibonacci> b;

	static arc::coro<CoroRecursiveCachedFibonacci> arc_make(arc::context & ctx, const int64_t & n)
	{
		throw_on_bad_input_for_fibonacci(n);

		if (n < 2)
			co_return { n };

		/**
		 * co_await is deferred to allow both dependencies to be computed in
		 * parallel because the tasks are launched eagerly
		 */
		arc::future futureA = ctx(CoroRecursiveCachedFibonacci::arc_make, n - 1);
		arc::future futureB = ctx(CoroRecursiveCachedFibonacci::arc_make, n - 2);

		arc::result a = co_await futureA;
		arc::result b = co_await futureB;

		/** co_await extracts the handle */
		CHECK(!futureA);
		CHECK(!futureB);

		co_return { a->value + b->value, std::move(a), std::move(b) };
	}
};

/**
 * Time Complexity: O(N).
 */
struct CoroLinearRecursiveFibonacci
{
	int64_t value = 0;
	std::optional<int64_t> previous;

	static arc::coro<CoroLinearRecursiveFibonacci> arc_make(arc::context & ctx, const int64_t & n)
	{
		throw_on_bad_input_for_fibonacci(n);

		if (n == 0)
			co_return { 0, std::nullopt };

		if (n == 1)
			co_return { 1, 0 };

		arc::result previous = co_await ctx(CoroLinearRecursiveFibonacci::arc_make, n - 1);

		int64_t value = previous->value + previous->previous.value_or(0);

		co_return { value, previous->value };
	}
};

TEST_CASE("Coro co_await", "[Coro]")
{
	arc::context ctx;

	arc::future a = ctx(CoroRecursiveFibonacci::arc_make, -1);
	CHECK_THROWS_AS(a.active_wait(), std::domain_error);

	arc::future b = ctx(CoroRecursiveFibonacci::arc_make, 93);
	CHECK_THROWS_AS(b.active_wait(), std::overflow_error);

	arc::future c = ctx(CoroRecursiveFibonacci::arc_make, 10);
	CHECK(c.active_wait()->value == int64_t(55));

	arc::future d = ctx(CoroRecursiveCachedFibonacci::arc_make, 92);
	CHECK(d.active_wait()->value == int64_t(7540113804746346429));

	arc::future e = ctx(CoroLinearRecursiveFibonacci::arc_make, 92);
	CHECK(e.active_wait()->value == int64_t(7540113804746346429));
}

TEST_CASE("Coro multithreaded", "[Coro]")
{
	arc::context ctx{ arc::options{ .workerThreadCount = 4 } };

	arc::future a = ctx(CoroRecursiveCachedFibonacci::arc_make, 92);

	/**
	 * NOTE: Disclaimer: since computing a fibonacci step is very quick, the
	 *       main thread will compute all steps before worker threads finish
	 *       initializing and have the opportunity to pick a task
	 */
	CHECK(a.active_wait()->value == int64_t(7540113804746346429));
}

TEST_CASE("Coro get_key", "[Coro]")
{
	arc::context ctx;
	arc::result result = ctx(CoroHelloMessage::arc_make, "Hello!").active_wait();

	CHECK(result.get_key<std::string, 0>() == "Hello!");

	/**
	 * NOTE: Undefined behavior, wrong key type
	 * result.get_key<int>();
	 */

	/** result referencing const char 'H' */
	arc::result<const char> alias{ &result->message[0], result };

	/** alias results retain the original key */
	CHECK(alias.get_key<std::string, 0>() == "Hello!");
}

struct CoroAwaitsAll
{
	static arc::coro<CoroAwaitsAll> arc_make(arc::context & ctx)
	{
		std::vector<arc::future<void>> jobs;

		jobs.emplace_back(ctx(CoroHelloWorld::arc_make));
		jobs.emplace_back(ctx(CoroDerivedResult1::arc_make));
		jobs.emplace_back(ctx(CoroRecursiveCachedFibonacci::arc_make, 5));

		std::vector results = co_await arc::all<void>{ ctx, jobs };

		co_return {};
	}
};

TEST_CASE("Coro wait all", "[Coro]")
{
	arc::context ctx;
	ctx(CoroAwaitsAll::arc_make).active_wait();
}

struct CoroInPlace : private arc::extra::non_copyable_non_movable
{
	CoroInPlace(int val)
		: val{ val }
	{}

	const int val = 0;

	static arc::coro<CoroInPlace> arc_make(arc::context & ctx)
	{
		arc::promise_proxy promise = co_await arc::get_promise_proxy<CoroInPlace>();
		CoroInPlace * result = promise.construct(5);
		co_return promise;
	}
};

TEST_CASE("Coro get_promise_proxy", "[Coro]")
{
	arc::context ctx;
	arc::result result = ctx(CoroInPlace::arc_make).active_wait();
	CHECK(result->val == 5);
}

struct CoroFunnelExample
{
	static arc::coro<CoroFunnelExample> arc_make(arc::context & ctx)
	{
		// clang-format off
		std::vector<std::string> jobKeys{
			"a00", "a01", "a02", "a03", "a04", "a05", "a06", "a07", "a08", "a09",
			"a10", "a11", "a12", "a13", "a14", "a15", "a16", "a17", "a18", "a19",
			"a10", "a11", "a12", "a13", "a14", "a15", "a16", "a17", "a18", "a19",
			"a10", "a11", "a12", "a13", "a14", "a15", "a16", "a17", "a18", "a19",
			"a00", "a01", "a02", "a03", "a04", "a05", "a06", "a07", "a08", "a09",
			"a10", "a11", "a12", "a13", "a14", "a15", "a16", "a17", "a18", "a19",
		};
		// clang-format on

		static constexpr size_t MAX_CONCURRENCY = 10;

		arc::funnel<CoroHelloMessage> funnel{ ctx };

		auto process = [](const CoroHelloMessage & m) { CHECK(m.message.size() == 3); };

		for (const std::string & key : jobKeys)
		{
			if (funnel.size() == MAX_CONCURRENCY)
				process(*co_await funnel);

			funnel.push(ctx(CoroHelloMessage::arc_make, key));
		}

		while (funnel.size())
			process(*co_await funnel);

		co_return {};
	}
};

TEST_CASE("Coro Funnel", "[Coro]")
{
	arc::context ctx{ arc::options{ .workerThreadCount = 3 } };
	arc::future a = ctx(CoroFunnelExample::arc_make);
	arc::result r = a.active_wait();
}

static std::string NonCoroFunction(arc::context & ctx) { return "Hello, World!"; }

TEST_CASE("Non-Coro Function", "[Coro]")
{
	arc::context ctx;
	arc::future future = ctx(NonCoroFunction);
	arc::result result = future.active_wait();
	CHECK(result);
	CHECK(*result == "Hello, World!");
}

static arc::coro<int64_t> NonCoroFibonacci(arc::context & ctx, const int64_t & n)
{
	throw_on_bad_input_for_fibonacci(n);

	if (n < 2)
		co_return int64_t{ n };

	std::vector abFutures{
		ctx(NonCoroFibonacci, n - 1),
		ctx(NonCoroFibonacci, n - 2),
	};

	std::vector ab = co_await arc::all<int64_t>{ ctx, abFutures };

	CHECK(ab.size() == 2);
	CHECK(ab[0]);
	CHECK(ab[1]);

	co_return { *ab[0] + *ab[1] };
}

TEST_CASE("Non-Coro Fibonacci", "[Coro]")
{
	arc::context ctx;
	arc::future future = ctx(NonCoroFibonacci, 10);
	arc::result result = future.active_wait();
	CHECK(result);
	CHECK(*result == 55);
}

static arc::coro<std::string> TwoArgumentsFunction(
	arc::context & ctx, const int64_t & i, const std::string & s)
{
	co_return s + std::to_string(i);
}

TEST_CASE("Two Arguments Function", "[Coro]")
{
	arc::context ctx;
	arc::result result0 = ctx(TwoArgumentsFunction, 5, "The number is: ").active_wait();
	arc::result result1 = ctx(TwoArgumentsFunction, 6, "The number is: ").active_wait();
	arc::result result2 = ctx(TwoArgumentsFunction, 5, "The value is: ").active_wait();
	arc::result result3 = ctx(TwoArgumentsFunction, 6, "The value is: ").active_wait();
	arc::result result4 = ctx(TwoArgumentsFunction, 5, "The number is: ").active_wait();
	arc::result result5 = ctx(TwoArgumentsFunction, 6, "The number is: ").active_wait();
	arc::result result6 = ctx(TwoArgumentsFunction, 5, "The value is: ").active_wait();
	arc::result result7 = ctx(TwoArgumentsFunction, 6, "The value is: ").active_wait();

	CHECK(*result0 == "The number is: 5");
	CHECK(*result1 == "The number is: 6");
	CHECK(*result2 == "The value is: 5");
	CHECK(*result3 == "The value is: 6");
	CHECK(*result4 == "The number is: 5");
	CHECK(*result5 == "The number is: 6");
	CHECK(*result6 == "The value is: 5");
	CHECK(*result7 == "The value is: 6");

	CHECK(result0.get() == result4.get());
	CHECK(result1.get() == result5.get());
	CHECK(result2.get() == result6.get());
	CHECK(result3.get() == result7.get());
}

arc::coro<int> f_0(arc::context & ctx) { co_return 0; }
arc::coro<int> f_1(arc::context & ctx) { co_return 1; }

/**
 * NOTE: A non-coroutine function that returns a coroutine is run under the
 *       context lock in the current implementation.
 */
arc::coro<int> f_select(arc::context & ctx, const int64_t & i)
{
	if (i == 0)
		return f_0(ctx);
	else
		return f_1(ctx);
}

TEST_CASE("NonCoro Return Coro", "[Coro]")
{
	arc::context ctx;
	ctx(f_select, 1).active_wait();
}

arc::coro<bool> LifetimeTrue(arc::context & ctx)
{
	ctx.set_caching_policy_global(co_await arc::get_self_reference<bool>());
	co_return true;
}

TEST_CASE("Global Coro", "[Coro]")
{
	arc::context ctx;

	arc::result r = ctx(LifetimeTrue).active_wait();

	/** Adding as either arc::result or arc::awaitable works. */
	ctx.set_caching_policy_global(r);
}
