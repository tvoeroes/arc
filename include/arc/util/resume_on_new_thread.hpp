#pragma once

#include <coroutine>
#include <thread>

namespace arc::util
{
	struct resume_on_new_thread;
}

struct arc::util::resume_on_new_thread
{
	std::jthread & thread;
	bool detach = false;

	bool await_ready() { return false; }
	void await_suspend(std::coroutine_handle<> handle)
	{
		/**
		 * NOTE: Members of "this" are stored in local variables because "*this"
		 *       can be destroyed at any point in time after the std::jthread
		 *       constructor is invoked, even before it returns.
		 */
		std::jthread & thread_ = thread;
		bool detach_ = detach;
		thread_ = std::jthread([handle] { handle.resume(); });
		if (detach_)
			thread_.detach();
	}
	void await_resume() {}
};
