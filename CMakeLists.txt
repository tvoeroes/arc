cmake_minimum_required(VERSION 3.24)

project(arc VERSION 0.0.0)

option(ARC_BUILD_TEST_PROGRAMS "Build the Arc test programs" OFF)
option(ARC_WITH_CATCH2 "Build test programs with Catch2" OFF)
option(ARC_WITH_TRACY "Enable Arc tracing" OFF)
option(ARC_WITH_SILENT_ABORT "Suppress the OS crash dialog when a check fails" OFF)

add_library(arc)

target_compile_features(arc PUBLIC cxx_std_23)

target_sources(arc
	PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
		include/arc/arc.hpp
		include/arc/arc/all.hpp
		include/arc/arc/context.hpp
		include/arc/arc/coro.hpp
		include/arc/arc/funnel.hpp
		include/arc/arc/future.hpp
		include/arc/arc/key_of.hpp
		include/arc/arc/options.hpp
		include/arc/arc/promise_proxy.hpp
		include/arc/arc/result.hpp
		include/arc/arc/task.hpp
		include/arc/detail/control_block.hpp
		include/arc/detail/coro_promise_base.hpp
		include/arc/detail/coro_promise.hpp
		include/arc/detail/handle.hpp
		include/arc/detail/key.hpp
		include/arc/detail/name_store.hpp
		include/arc/detail/reflect.hpp
		include/arc/detail/result_store.hpp
		include/arc/detail/scheduler.hpp
		include/arc/detail/store.hpp
		include/arc/detail/zone_info.hpp
		include/arc/impl/arc.ipp
		include/arc/util/algorithms.hpp
		include/arc/util/check.hpp
		include/arc/util/debug.hpp
		include/arc/util/guard.hpp
		include/arc/util/non_copyable_non_movable.hpp
		include/arc/util/on_scope_exit.hpp
		include/arc/util/resume_on_new_thread.hpp
		include/arc/util/tracing.hpp
		include/arc/util/util.hpp
	PRIVATE
		src/arc.cpp
)

if (EMSCRIPTEN)
	target_compile_definitions(arc PUBLIC arc_PLATFORM_IS_EMSCRIPTEN=1 arc_PLATFORM_IS_WINDOWS=0 arc_PLATFORM_IS_LINUX=0)
elseif(WIN32)
	target_compile_definitions(arc PUBLIC arc_PLATFORM_IS_EMSCRIPTEN=0 arc_PLATFORM_IS_WINDOWS=1 arc_PLATFORM_IS_LINUX=0)
elseif(UNIX)
	target_compile_definitions(arc PUBLIC arc_PLATFORM_IS_EMSCRIPTEN=0 arc_PLATFORM_IS_WINDOWS=0 arc_PLATFORM_IS_LINUX=1)
else()
	message(FATAL_ERROR "arc: Unsupported platform")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_definitions(arc PUBLIC arc_COMPILER_IS_CLANG=1 arc_COMPILER_IS_GCC=0 arc_COMPILER_IS_MSVC=0)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_definitions(arc PUBLIC arc_COMPILER_IS_CLANG=0 arc_COMPILER_IS_GCC=1 arc_COMPILER_IS_MSVC=0)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(arc PUBLIC arc_COMPILER_IS_CLANG=0 arc_COMPILER_IS_GCC=0 arc_COMPILER_IS_MSVC=1)
else()
	message(FATAL_ERROR "arc: Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(ARC_WITH_SILENT_ABORT)
	target_compile_definitions(arc PUBLIC arc_CHECK_SILENT_ABORT=1)
else()
	target_compile_definitions(arc PUBLIC arc_CHECK_SILENT_ABORT=0)
endif()

if(ARC_WITH_TRACY)
	find_package(Tracy REQUIRED)
	target_link_libraries(arc PUBLIC Tracy::TracyClient)
	target_compile_definitions(arc PUBLIC arc_TRACE_INSTRUMENTATION_ENABLE=1)
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
		target_compile_definitions(arc_api_test PUBLIC arc_WITH_CATCH2=1)
		catch_discover_tests(arc_api_test)
	endif()

	add_executable(folder_size tests/folder_size.cpp)
	target_link_libraries(folder_size PRIVATE arc)

	add_executable(arc_example_1 tests/arc_example_1.cpp)
	target_link_libraries(arc_example_1 PRIVATE arc)

	add_executable(arc_example_2 tests/arc_example_2.cpp)
	target_link_libraries(arc_example_2 PRIVATE arc)
endif()
