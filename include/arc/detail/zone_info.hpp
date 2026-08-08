#pragma once

#if arc_TRACE_INSTRUMENTATION_ENABLE
	#include <tracy/TracyC.h>
#endif

namespace arc::detail
{
	class zone_info;
}

#if arc_TRACE_INSTRUMENTATION_ENABLE

/**
 * Describes the tracing zone to open around a task.
 *
 * Holds either a source location record with unlimited lifetime ("named"), or a name that the
 * profiler has to copy into an allocation of its own for every zone ("transient"). Named zones
 * cost nothing per zone but need every string to outlive the process.
 *
 * Converting to bool answers whether a zone should be opened at all. An empty name means the
 * entity opens its own zones and the worker must not add one around it; a null name means
 * nothing is known about the entity.
 */
class arc::detail::zone_info
{
public:
	zone_info() = default;

	/** Implicit, so a name can be passed wherever a zone_info is expected. */
	zone_info(const char * transientName) noexcept
		: name_{ transientName }
	{}

	/**
	 * `namedLocation`, and every string it points to, must outlive the process. Both are handed
	 * to the profiler, which reads them long after the zone ended and never frees them.
	 *
	 * The name is taken from the `function` field: a null `name` is how tracy spells "label this
	 * zone with the function it is in", which is the shape named records are built in, so
	 * `namedLocation.name` would be null here.
	 */
	explicit zone_info(const ___tracy_source_location_data & namedLocation) noexcept
		: name_{ namedLocation.function }
		, location_{ &namedLocation }
	{}

	explicit operator bool() const noexcept { return name_ && *name_ != '\0'; }

	bool is_named() const noexcept { return location_ != nullptr; }

	/** Only meaningful while is_named(). */
	const ___tracy_source_location_data & named_location() const noexcept { return *location_; }

	/** Non-null whenever the conversion to bool is true. */
	const char * transient_name() const noexcept { return name_; }

private:
	const char * name_ = nullptr;
	const ___tracy_source_location_data * location_ = nullptr;
};

/**
 * Reports, once per process, that named zones have degraded to transient ones because the
 * compiler gave two call sites the same closure type for a defaulted lambda template argument.
 *
 * See arc_TRACE_MAKE_ZONE_INFO for what detects this and why it is not fatal.
 */
void arc_TRACE_REPORT_SHARED_CLOSURE_TYPE();

#else

class arc::detail::zone_info
{
public:
	zone_info() = default;

	zone_info(const char *) noexcept {}

	explicit operator bool() const noexcept { return false; }
};

#endif
