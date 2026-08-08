#include "arc/arc.hpp"

#include <cstdlib>
#include <filesystem>
#include <print>
#include <vector>

struct CoroFilesystemEntrySize
{
	uint64_t allFilesSizeBytes = 0;
	uint64_t folderCount = 0;
	uint64_t fileCount = 0;
	uint64_t unsupportedEntries = 0;

	void add_child(const CoroFilesystemEntrySize & child)
	{
		allFilesSizeBytes += child.allFilesSizeBytes;
		folderCount += child.folderCount;
		fileCount += child.fileCount;
		unsupportedEntries += child.unsupportedEntries;
	}

	void print_counts() const
	{
		std::println("allFilesSizeBytes = {}", allFilesSizeBytes);
		std::println("folderCount = {}", folderCount);
		std::println("fileCount = {}", fileCount);
		std::println("unsupportedEntries = {}", unsupportedEntries);
	}
};

static std::string to_key(const std::filesystem::path & path)
{
	return path.lexically_normal().string();
}

static arc::coro<CoroFilesystemEntrySize> get_filesystem_entry_size(
	arc::context & ctx, const std::string & path)
{
	CoroFilesystemEntrySize result;

	std::vector<arc::future<CoroFilesystemEntrySize>> subdirs;

	for (const auto & entry : std::filesystem::directory_iterator{ path })
	{
		if (entry.is_directory())
		{
			subdirs.emplace_back(ctx[get_filesystem_entry_size, to_key(entry.path())]);
			result.folderCount += 1;
		}
		else if (entry.is_regular_file())
		{
			result.allFilesSizeBytes += entry.file_size();
			result.fileCount += 1;
		}
		else
		{
			result.unsupportedEntries += 1;
		}
	}

	std::vector subdirResults = co_await arc::all<CoroFilesystemEntrySize>{ ctx, subdirs };

	for (const auto & subdir : subdirResults)
		result.add_child(*subdir);

	co_return result;
}

/**
 * This program uses 10 threads to traverse the filesystem and compile a summary.
 */
int main(int argc, char * argv[])
{
	if (argc < 2)
	{
		std::println(stderr, "Missing argument.");
		return EXIT_FAILURE;
	}

	try
	{
		arc::options options;
		options.workerThreadCount = 9;

		arc::context ctx{ options };

		const std::string key = to_key(argv[1]);

		arc::future future = ctx[get_filesystem_entry_size, key];
		arc::result result = future.active_wait();

		result->print_counts();
	}
	catch (const std::exception & e)
	{
		std::println(stderr, "{}", e.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::println(stderr, "Unknown exception thrown.");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
