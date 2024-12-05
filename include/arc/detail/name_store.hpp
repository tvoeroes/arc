#pragma once

#include "arc/util/check.hpp"
#include "arc/util/std.hpp"

#if arc_TRACE_INSTRUMENTATION_ENABLE
struct arc::detail::name_store
{
public:
	const char * get_name(std::coroutine_handle<> handle) const
	{
		if (void * address = handle.address())
		{
			auto namesIt = names.ReadOnly();
			if (auto it = namesIt->find(address); it != namesIt->end())
				return it->second;
		}
		else
		{
			return "[nullptr]";
		}
		return "[?]";
	}

	void set_name(std::coroutine_handle<> handle, const char * name)
	{
		if (void * address = handle.address())
		{
			if (name)
			{
				auto namesIt = names.ReadAndWrite();
				(*namesIt)[address] = name;
			}
			else
			{
				size_t erased = names.ReadAndWrite()->erase(address);
				arc_CHECK_Precondition(erased == 1);
			}
		}
	}

private:
	arc::extra::shared_guard<std::unordered_map<void *, const char *>> names;
};
#else
struct arc::detail::name_store
{
public:
	const char * get_name(std::coroutine_handle<> handle) const { return ""; }
	void set_name(std::coroutine_handle<> handle, const char * name) {}
};
#endif
