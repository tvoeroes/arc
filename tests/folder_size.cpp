#include "arc/arc.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

struct CoroFilesystemEntrySize
{
	uint64_t allFilesSizeBytes = 0;
	uint64_t folderCount = 0;
	uint64_t fileCount = 0;
	uint64_t unsupportedEntries = 0;

	void AddChild(const CoroFilesystemEntrySize & child)
	{
		allFilesSizeBytes += child.allFilesSizeBytes;
		folderCount += child.folderCount;
		fileCount += child.fileCount;
		unsupportedEntries += child.unsupportedEntries;
	}

	void PrintCounts() const
	{
		std::cout << "allFilesSizeBytes = " << allFilesSizeBytes << "\n";
		std::cout << "folderCount = " << folderCount << "\n";
		std::cout << "fileCount = " << fileCount << "\n";
		std::cout << "unsupportedEntries = " << unsupportedEntries << std::endl;
	}

	static std::string ToKey(const std::filesystem::path & path)
	{
		return path.lexically_normal().string();
	}

	static arc::coro<CoroFilesystemEntrySize> arc_make(arc::context & ctx, const std::string & path)
	{
		CoroFilesystemEntrySize result;

		std::vector<arc::future<CoroFilesystemEntrySize>> subdirs;

		for (const auto & entry : std::filesystem::directory_iterator{ path })
		{
			if (entry.is_directory())
			{
				subdirs.emplace_back(ctx(CoroFilesystemEntrySize::arc_make, ToKey(entry.path())));
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
			result.AddChild(*subdir);

		co_return result;
	}
};

/**
 * This program uses 10 threads to traverse the filesystem and compile a summary.
 */
int main(int argc, char * argv[])
{
	if (argc < 2)
	{
		std::cerr << "Missing argument.\n";
		return EXIT_FAILURE;
	}

	try
	{
		arc::options options;
		options.workerThreadCount = 9;

		arc::context ctx{ options };

		const std::string key = CoroFilesystemEntrySize::ToKey(argv[1]);

		arc::future future = ctx(CoroFilesystemEntrySize::arc_make, key);
		arc::result result = future.active_wait();

		result->PrintCounts();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << '\n';
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::cerr << "Unknown exception thrown.\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
