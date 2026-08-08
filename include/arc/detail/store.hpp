#pragma once

#include "arc/detail/control_block.hpp"
#include "arc/detail/handle.hpp"
#include "arc/detail/key.hpp"
#include "arc/util/guard.hpp"
#include "arc/util/tracing.hpp"
#include "arc/util/util.hpp"

#include <queue>
#if arc_WITH_SOURCE_LOCATION
	#include <source_location>
#endif

namespace arc::detail
{
	struct store;
}

struct arc::detail::store
{
public:
	store();
	~store();

	arc::detail::handle retrieve_reference(
		arc::detail::key && key
#if arc_WITH_SOURCE_LOCATION
		,
		const std::source_location & sourceLocation
#endif
	);

	void release_reference(arc::detail::handle && coroHandle);

	void set_empty_once_callback(arc::function<void()> && emptyOnceCallback);

private:
	struct Data
	{
		arc_TRACE_CONTAINER_UNORDERED_MAP(arc::detail::key, arc::detail::control_block) store;
		std::queue<arc::function<void()>> emptyOnceCallbacks;
	};

	static_assert(std::is_same_v<arc::detail::store_entry, decltype(Data{}.store)::value_type>);

private:
	arc::util::recursive_guard<Data> data;
};
