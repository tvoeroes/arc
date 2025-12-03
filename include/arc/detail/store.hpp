#pragma once

#include "arc/arc/key_of.hpp"
#include "arc/detail/control_block.hpp"
#include "arc/detail/key.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"
#include "arc/util/tracing.hpp"

struct arc::detail::store
{
public:
	store();
	~store();

	arc::detail::handle retrieve_reference(arc::detail::key && key arc_SOURCE_LOCATION_ARG);

	void release_reference(arc::detail::handle && coroHandle);

	void set_empty_once_callback(arc::function<void()> && emptyOnceCallback);

private:
	struct Data
	{
		arc_TRACE_CONTAINER_UNORDERED_MAP(arc::detail::key, arc::detail::control_block) store;
		std::queue<arc::function<void()>> emptyOnceCallbacks;
	};

	static_assert(std::is_same_v<arc::detail::store_entry, decltype(Data{}.store)::value_type>);

	arc::extra::recursive_guard<Data> data;
};
