#pragma once

#include "arc/detail/zone_info.hpp"

#include <string_view>

namespace arc::detail
{
	inline constexpr const char * fallback_name = "[?]";
#if arc_TRACE_INSTRUMENTATION_ENABLE
	/**
	 * Returns the zone info for an address. If none has been set, returns fallback instead.
	 */
	arc::detail::zone_info get_zone_info(
		const void * address, arc::detail::zone_info fallback = fallback_name);
	/**
	 * Sets the zone info for an address.
	 *
	 * An empty name ("") means the entity associated with this address handles its own tracing
	 * scopes internally. The worker should not create a trace scope for it. A non-empty name
	 * means the worker should create a tracing scope from it.
	 */
	void set_zone_info(const void * address, arc::detail::zone_info info);
	/**
	 * Removes any zone info for an address. A named zone info outlives this call: the source
	 * location record it points to is never freed.
	 */
	void clear_zone_info(const void * address);
#else
	inline arc::detail::zone_info get_zone_info(
		const void * address, arc::detail::zone_info fallback = fallback_name)
	{
		return fallback;
	}
	inline void set_zone_info(const void * address, arc::detail::zone_info info) {}
	inline void clear_zone_info(const void * address) {}
#endif
	/**
	 * Returns a copy of the input argument string. Is terminated with '\0'. Is never deallocated.
	 */
	const char * leak_new_c_string(std::string_view string);
}
