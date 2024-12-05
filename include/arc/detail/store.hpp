#pragma once

#include "arc/arc/key_of.hpp"
#include "arc/detail/control_block.hpp"
#include "arc/detail/key.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::detail::store
{
public:
	store();
	~store();

	template <typename F>
	arc::future<arc::result_of_t<F>> retrieve_reference(
		arc::context & ctx, F * f, arc::detail::key && key);

	void release_reference(arc::detail::handle && coroHandle);

	void set_empty_once_callback(std::move_only_function<void()> && emptyOnceCallback);

private:
	struct Data
	{
		arc_TRACE_CONTAINER_UNORDERED_MAP(arc::detail::key, arc::detail::control_block) store;
		std::queue<std::move_only_function<void()>> emptyOnceCallbacks;
	};

	static_assert(std::is_same_v<arc::detail::store_entry, decltype(Data{}.store)::value_type>);

	arc::extra::recursive_guard<Data> data;
};
