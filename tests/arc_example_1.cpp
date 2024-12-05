#include "arc/arc.hpp"

#include <filesystem>
#include <stdint.h>
#include <string>
#include <vector>

/**
 * ========================================================
 * ==== NOTE: Update README.md when changing this file ====
 * ========================================================
 */

struct MyImage
{
	const std::vector<uint8_t> image;

	static arc::coro<MyImage> make(arc::context & ctx, const std::string & key)
	{
		co_return { key };
	}

private:
	MyImage(const std::filesystem::path & path) { /* ... */ }
};

arc::coro<int> process_images(arc::context & ctx)
{
	std::vector<arc::future<MyImage>> futures{
		ctx(MyImage::make, "image1.png"),
		ctx(MyImage::make, "image2.png"),
		ctx(MyImage::make, "image3.png"),
	};

	std::vector loadedImages = co_await arc::all<MyImage>{ ctx, futures };

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
