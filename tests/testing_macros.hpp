#pragma once

#if __has_include(<catch2/catch_test_macros.hpp>)
	#include <catch2/catch_test_macros.hpp>
#else
	#include <cassert>
	#include <vector>

static std::vector<void (*)()> tests_;

struct Registrator
{
	Registrator(void (*function)()) { tests_.emplace_back(function); }
};

int main()
{
	for (const auto & test : tests_)
		test();
}

	#define CONCAT2(a, b) a##b
	#define CONCAT(a, b) CONCAT2(a, b)
	#define TEST_CASE_IMPL(functionName)                                                           \
		static void functionName();                                                                \
		static Registrator CONCAT2(functionName, Registrator){ functionName };                     \
		static void functionName()
	#define TEST_CASE(...) TEST_CASE_IMPL(CONCAT(test_, __COUNTER__))
	#define CHECK(...) assert(static_cast<bool>(__VA_ARGS__))
	#define REQUIRE(...) assert(static_cast<bool>(__VA_ARGS__))
	#define CHECK_FALSE(arg) assert(static_cast<bool>(arg) == false)
	#define SECTION(...) /** FIXME: this doesn't match how Catch2 works */
	#define CHECK_THROWS_AS(expression, exception)                                                 \
		do                                                                                         \
		{                                                                                          \
			try                                                                                    \
			{                                                                                      \
				expression;                                                                        \
			}                                                                                      \
			catch (const exception &)                                                              \
			{                                                                                      \
				assert(true);                                                                      \
			}                                                                                      \
			catch (...)                                                                            \
			{                                                                                      \
				assert(false);                                                                     \
			}                                                                                      \
		} while (false)
#endif
