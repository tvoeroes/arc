cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 23)

project(arc VERSION 0.0.0)

option(ARC_BUILD_TEST_PROGRAMS "Build the Arc test programs" OFF)
option(ARC_WITH_CATCH2 "Build test programs with Catch2" OFF)
option(ARC_WITH_TRACY "Enable Arc tracing" OFF)

add_library(arc)

target_include_directories(arc PUBLIC include)

target_sources(arc
	PUBLIC
		include/arc/arc.hpp
		include/arc/impl/arc.ipp
		include/arc/arc/all.hpp
		include/arc/arc/context.hpp
		include/arc/arc/coro.hpp
		include/arc/arc/funnel.hpp
		include/arc/arc/future.hpp
		include/arc/arc/key_of.hpp
		include/arc/arc/options.hpp
		include/arc/arc/promise_proxy.hpp
		include/arc/arc/result.hpp
		include/arc/detail/control_block.hpp
		include/arc/detail/handle.hpp
		include/arc/detail/key.hpp
		include/arc/detail/name_store.hpp
		include/arc/detail/promise_base.hpp
		include/arc/detail/promise.hpp
		include/arc/detail/reflect.hpp
		include/arc/detail/scheduler.hpp
		include/arc/detail/store.hpp
		include/arc/extra/algorithms.hpp
		include/arc/extra/guard.hpp
		include/arc/extra/non_copyable_non_movable.hpp
		include/arc/extra/on_scope_exit.hpp
		include/arc/extra/task.hpp
		include/arc/fwd.hpp
		include/arc/util/check.hpp
		include/arc/util/debug.hpp
		include/arc/util/platform.hpp
		include/arc/util/std.hpp
		include/arc/util/tracing.hpp
		include/arc/util/util.hpp
	PRIVATE
		src/arc.cpp
)

if(ARC_WITH_TRACY)
	find_package(Tracy REQUIRED)
	target_link_libraries(arc PUBLIC Tracy::TracyClient)
endif()

if(ARC_BUILD_TEST_PROGRAMS)
	add_executable(arc_api_test
		tests/arc_api_test.cpp
		tests/testing_macros.hpp
	)

	target_link_libraries(arc_api_test PRIVATE arc)

	if(ARC_WITH_CATCH2)
		find_package(Catch2 REQUIRED)
		target_link_libraries(arc_api_test PRIVATE Catch2::Catch2WithMain)
		catch_discover_tests(arc_api_test)
	endif()

	add_executable(folder_size tests/folder_size.cpp)
	target_link_libraries(folder_size PRIVATE arc)

	add_executable(arc_example_1 tests/arc_example_1.cpp)
	target_link_libraries(arc_example_1 PRIVATE arc)

	add_executable(arc_example_2 tests/arc_example_2.cpp)
	target_link_libraries(arc_example_2 PRIVATE arc)
endif()

install(
	DIRECTORY
		./
	DESTINATION
		./
	COMPONENT
		arc-repository
	EXCLUDE_FROM_ALL
)
