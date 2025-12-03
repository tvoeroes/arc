# Awaitable Result Context

Arc is a C++ library that simplifies parallel programming with focus on
caching and reusing the results of computations.

## Examples

The following code demonstrates how one could load three images on three threads
in parallel without blocking any threads and use the images as soon as all three
have finished loading. If for example a different part of the program requests
the same images, then the image objects will be shared instead of being
recalculated:

``` c++
struct MyImage
{
	std::vector<uint8_t> image;

	static arc::coro<const MyImage> make(arc::context & ctx, const std::string & key)
	{
		co_return { key };
	}

private:
	MyImage(const std::filesystem::path & path) { /* ... */ }
};

arc::coro<const int> process_images(arc::context & ctx)
{
	std::vector<arc::future<const MyImage>> futures{
		ctx(MyImage::make, "image1.png"),
		ctx(MyImage::make, "image2.png"),
		ctx(MyImage::make, "image3.png"),
	};

	std::vector loadedImages = co_await arc::all<const MyImage>{ ctx, futures };

	/* ... */

	co_return loadedImages.size();
}

int main()
{
	arc::options options{ .workerThreadCount = 2 };
	arc::context ctx{ options };
	arc::future future = ctx(process_images);
	arc::result result = future.active_wait();
}
```

For more examples see [arc_example_2.cpp](tests/arc_example_2.cpp),
[arc_api_test.cpp](tests/arc_api_test.cpp) or
[folder_size.cpp](tests/folder_size.cpp).


## API

``` c++
#include <arc/arc.hpp>
```

See [fwd.hpp](include/arc/fwd.hpp) for an overview.

Also see [context.hpp](include/arc/arc/context.hpp),
[future.hpp](include/arc/arc/future.hpp) and
[result.hpp](include/arc/arc/result.hpp).

The library uses the namespaces `arc::*` and `arc_*`. The API is not stable
across versions.

## Result Lifetime

The current behavior is that once an `arc::future` has been created, the
corresponding value will be computed even if the reference count drops to zero
before the value finishes computing a.k.a. there is no cancellation support.
When there are no more references to that value, the value may be destroyed at
any time but at latest, the value will be destroyed when the `arc::context` is
destroyed and there are no dependency cycles left (in the case of a dependency
cycle a deadlock occurs). It can happen that the reference count drops to zero
and is then increased from zero. In this case the existing value may be reused
or if the destruction has already begun, the destruction will complete and a new
instance will begin being computed. There are no lifetime overlaps for a value
with the same function.

## Result Key

The results key is a combination of the function arguments values and the
function pointer.

## Integration

Arc is primarily designed to be included via the CMake `add_subdirectory()`
mechanism. Note that the library is developed as part of another monorepo.

### Catch2

Some test programs will link to the Catch2 library if the parent CMake project
loaded the library details (typically via `find_package(Catch2)`).

### Tracing via Tracy

If the parent CMake project has loaded the Tracy library details (typically via
`find_package(Tracy)`) the library will link to it and insert tracing info. Note
that trace scopes across coroutine suspension points are not implemented any may
result in profiler crashes or broken output.

## Folder Structure

* **include/arc/arc**: The library consumer should only access variables,
  functions and types from this location. The API may change at any new version.
* **include/arc/arc.hpp**: This is the only file that the the library consumer
  should include.

## About The Project Name

The Awaitable is part of the library name because `arc::future` used to be
called `arc::awaitable` and arc just rolls off the tongue so it is staying that
way.

## Changelog

### 0.3

* Fix `arc_TRACE_MESSAGE_T` message and `arc_TRACE_LOCKABLE_RENAME_T` name
  lifetime.
* Const results support: `arc::coro<const T>`, `arc::future<const T>` and
  `arc::result<const T>`.
* `co_return const_variable;` support for `arc::coro`.
* Support for `enum class` as key (int64_t only).
