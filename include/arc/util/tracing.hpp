#pragma once

#include "arc/detail/name_store.hpp"
#include "arc/util/algorithms.hpp"
#include "arc/util/non_copyable_non_movable.hpp"

#include <queue>
#include <source_location>
#include <stack>
#include <stdint.h>
#include <type_traits>
#include <unordered_map>

inline constexpr uint32_t arc_TRACE_COLOR_MAKE(uint8_t r, uint8_t g, uint8_t b)
{
	return uint32_t(r) << uint32_t(16) | uint32_t(g) << uint32_t(8) | uint32_t(b);
}

inline constexpr uint32_t arc_TRACE_COLOR_MAKE(std::string_view str)
{
	uint32_t hash = arc::util::FNV_1a_32(str);

	uint32_t va = hash & 0xffu;

	uint32_t lo = 0x00u;
	uint32_t hi = 0xffu;

	switch ((hash >> 8u) % 6u) /** not a perfectly uniform distribution */
	{
	case 0:
		return arc_TRACE_COLOR_MAKE(lo, hi, va);
	case 1:
		return arc_TRACE_COLOR_MAKE(hi, lo, va);
	case 2:
		return arc_TRACE_COLOR_MAKE(lo, va, hi);
	case 3:
		return arc_TRACE_COLOR_MAKE(hi, va, lo);
	case 4:
		return arc_TRACE_COLOR_MAKE(va, lo, hi);
	default:
		return arc_TRACE_COLOR_MAKE(va, hi, lo);
	}
}

/** -- GLOBAL SWITCHES -------------------------------------------------------------------------- */
static constexpr bool arc_TRACE_PLOT_ENABLED = true;
static constexpr bool arc_TRACE_CORO = true;
static constexpr bool arc_TRACE_GRAPH = true;
static constexpr bool arc_TRACE_GRAPH_SLEEP = true;
static constexpr bool arc_TRACE_MEMORY = true;
static constexpr bool arc_TRACE_OTHER = true;
static constexpr bool arc_TRACE_RENDER = true;
static constexpr bool arc_TRACE_FILE_IO = true;
static constexpr bool arc_TRACE_WORKER_IDLE = false;
static constexpr bool arc_TRACE_EXPORT_CONFIG = false;

/**
 * Probe for the compiler property named zones are built on: two uses of a defaulted lambda
 * template argument must yield two distinct closure types.
 *
 * Necessary but not sufficient. It exercises the simplest form of the trick, not the promise
 * constructor form arc_TRACE_MAKE_ZONE_INFO is actually used through, which is why that function
 * checks again at run time.
 */
template <auto = [] {}>
struct arc_TRACE_UNIQUE_CLOSURE_PROBE
{};

static constexpr bool arc_TRACE_NAMED_ZONES_SUPPORTED =
	!std::is_same_v<arc_TRACE_UNIQUE_CLOSURE_PROBE<>, arc_TRACE_UNIQUE_CLOSURE_PROBE<>>;

/**
 * Whether zones may reference a source location record with unlimited lifetime instead of having
 * the profiler allocate one per zone. Set the leading operand to false to force every zone onto
 * the transient path.
 */
static constexpr bool arc_TRACE_NAMED_ZONES = true && arc_TRACE_NAMED_ZONES_SUPPORTED;

#if arc_TRACE_INSTRUMENTATION_ENABLE

	#include "arc/util/algorithms.hpp"

	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>

	#define arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(functionName, constNoexceptVolatileRef)      \
		template <typename... Args>                                                                \
		decltype(auto) functionName(Args &&... args) constNoexceptVolatileRef                      \
		{                                                                                          \
			return container.functionName(std::forward<Args>(args)...);                            \
		}

class arc_TRACE_CONTAINER_BASE
{
public:
	arc_TRACE_CONTAINER_BASE() = default;

	/**
	 * Must be called before fist use of the container.
	 */
	void Configure(std::string_view name_)
	{
		name = arc::detail::leak_new_c_string(name_);
		TracyPlotConfig(name, tracy::PlotFormatType::Number, true, true, 0);
		TracyPlot(name, int64_t(0));
	}

	void Plot(int64_t value) { TracyPlot(name, value); }

	~arc_TRACE_CONTAINER_BASE() { TracyPlot(name, int64_t(0)); }

private:
	const char * name = "[Unnamed Containers]";
};

template <typename Key, typename T>
class arc_TRACE_unordered_map : public arc_TRACE_CONTAINER_BASE
{
public:
	using wrapped_type = std::unordered_map<Key, T>;
	using value_type = wrapped_type::value_type;

public:
	arc_TRACE_unordered_map() = default;

	template <typename... Args>
	decltype(auto) try_emplace(Args &&... args)
	{
		decltype(auto) result = container.try_emplace(std::forward<Args>(args)...);
		if (result.second)
			Plot(int64_t(container.size()));
		return result;
	}

	template <typename... Args>
	decltype(auto) emplace(Args &&... args)
	{
		decltype(auto) result = container.emplace(std::forward<Args>(args)...);
		if (result.second)
			Plot(int64_t(container.size()));
		return result;
	}

	template <typename... Args>
	decltype(auto) erase(Args &&... args)
	{
		decltype(auto) result = container.erase(std::forward<Args>(args)...);
		Plot(int64_t(container.size()));
		return result;
	}

	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(find, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(find, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(end, const noexcept);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(size, const noexcept);

private:
	wrapped_type container;
};

template <typename T>
class arc_TRACE_queue : public arc_TRACE_CONTAINER_BASE
{
public:
	using wrapped_type = std::queue<T>;
	using value_type = wrapped_type::value_type;

public:
	arc_TRACE_queue() = default;

	template <typename... Args>
	decltype(auto) emplace(Args &&... args)
	{
		decltype(auto) result = container.emplace(std::forward<Args>(args)...);
		Plot(int64_t(container.size()));
		return result;
	}

	void pop()
	{
		container.pop();
		Plot(int64_t(container.size()));
	}

	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(size, const noexcept);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(front, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(front, );

private:
	wrapped_type container;
};

template <typename T>
class arc_TRACE_stack : public arc_TRACE_CONTAINER_BASE
{
public:
	using wrapped_type = std::stack<T>;
	using value_type = wrapped_type::value_type;

public:
	arc_TRACE_stack() = default;

	template <typename... Args>
	decltype(auto) emplace(Args &&... args)
	{
		decltype(auto) result = container.emplace(std::forward<Args>(args)...);
		Plot(int64_t(container.size()));
		return result;
	}
	void pop()
	{
		container.pop();
		Plot(int64_t(container.size()));
	}

	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(top, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(top, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(size, const);

private:
	wrapped_type container;
};

template <typename T>
class arc_TRACE_vector : public arc_TRACE_CONTAINER_BASE
{
public:
	using wrapped_type = std::vector<T>;
	using value_type = wrapped_type::value_type;

public:
	using iterator = wrapped_type::iterator;

	arc_TRACE_vector() = default;

	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(size, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(begin, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(end, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(insert, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(front, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(erase, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(operator[], const);

private:
	wrapped_type container;
};

inline void arc_TRACE_INTERNAL_MESSAGE_T_IMPL(std::string_view message)
{
	TracyMessage(message.data(), message.size());
}

inline void arc_TRACE_INTERNAL_LOCKABLE_RENAME_T_IMPL(auto & lock, std::string_view name)
{
	LockableName(lock, name.data(), name.size());
}

inline std::string_view arc_TRACE_INTERNAL_TRUNCATE_FUNCTION_NAME(std::string_view name)
{
	constexpr std::string_view token = "__cdecl ";
	if (auto pos = name.find(token); pos != std::string_view::npos)
		name.remove_prefix(pos + token.size());
	return name;
}

inline TracyCZoneCtx arc_TRACE_ZONE_BEGIN(std::string_view name) noexcept
{
	if (name.empty())
		name = "[?]";
	uint64_t srcloc = ___tracy_alloc_srcloc_name(
		0, "", 0, "", 0, name.data(), name.size(), arc_TRACE_COLOR_MAKE(name));
	return ___tracy_emit_zone_begin_alloc(srcloc, 1);
}

inline void arc_TRACE_ZONE_END(TracyCZoneCtx ctx) noexcept { ___tracy_emit_zone_end(ctx); }

/**
 * Zone description naming the function `loc` refers to, backed by a source location record with
 * unlimited lifetime.
 *
 * The strings behind `loc` are stored by pointer and handed to the profiler, which never frees
 * them. std::source_location exposes strings with static storage duration, so they qualify; only
 * the std::source_location object itself is short-lived, and nothing keeps a pointer to it.
 *
 * `loc` is deliberately not defaulted. The location worth recording is the one of the function
 * being described, which is always further up than this call; a default would capture the caller
 * instead and name every zone after it. Callers get `loc` from a defaulted parameter of their own.
 *
 * WARNING: one record per call site relies on the compiler giving the defaulted `Tag` a fresh
 * closure type at every point of use. The standard never promises that, and GCC reads it the other
 * way: one lambda written once is one closure type, however many call sites reach it (GCC PR
 * 115722, unresolved). A compiler that shares the type hands every caller the record of whichever
 * call site ran first, labeling every coroutine with the name of an unrelated one. The mismatch
 * branch below detects exactly that and degrades to a transient zone, so getting this wrong costs
 * performance, never correctness.
 */
template <auto Tag = [] {}>
arc::detail::zone_info arc_TRACE_MAKE_ZONE_INFO(const std::source_location & loc)
{
	if constexpr (!arc_TRACE_NAMED_ZONES)
	{
		return arc::detail::zone_info{ loc.function_name() };
	}
	else
	{
		/** A null `name` tells tracy to label the zone with `function`. */
		static const ___tracy_source_location_data data{
			nullptr,
			loc.function_name(),
			loc.file_name(),
			loc.line(),
			arc_TRACE_COLOR_MAKE(loc.function_name()),
		};

		/**
		 * A different call site initialized this record, see the warning above. Comparing pointers
		 * rather than string contents can also report a mismatch for one function reaching this
		 * through two translation units, which takes the same harmless transient path.
		 */
		if (data.function != loc.function_name() || data.file != loc.file_name() ||
			data.line != loc.line())
		{
			arc_TRACE_REPORT_SHARED_CLOSURE_TYPE();
			return arc::detail::zone_info{ loc.function_name() };
		}

		return arc::detail::zone_info{ data };
	}
}

/**
 * Opens a tracing zone for the lifetime of the object.
 */
class arc_TRACE_ZONE_SCOPE
{
public:
	arc_TRACE_ZONE_SCOPE(const arc::detail::zone_info & info, bool active) noexcept
		: ctx{ begin(info, active) }
	{}

	arc_NON_COPYABLE_NON_MOVABLE(arc_TRACE_ZONE_SCOPE);

	~arc_TRACE_ZONE_SCOPE() noexcept { ___tracy_emit_zone_end(ctx); }

private:
	static TracyCZoneCtx begin(const arc::detail::zone_info & info, bool active) noexcept
	{
		/** A zeroed context is an inactive one, which ___tracy_emit_zone_end ignores. */
		if (!active)
			return TracyCZoneCtx{};

		if (info.is_named())
			return ___tracy_emit_zone_begin(&info.named_location(), 1);
		else
			/**
			 * lighter, saves space compared to sending the full file name, function name and line
			 */
			return arc_TRACE_ZONE_BEGIN(info.transient_name());
	}

private:
	TracyCZoneCtx ctx{};
};

	/** -- EVENT -------------------------------------------------------------------------------- */
	/**
	 *  `name` must be a string literal. Use the -_T version with other strings.
	 */
	#define arc_TRACE_EVENT_SCOPED_NC(active, name, color)                                         \
		ZoneNamedNC(arc_TRACE_INFO_STORAGE, name, color, active)

	/**
	 *  `name` must be a string literal. Use the -_T version with other strings.
	 */
	#define arc_TRACE_EVENT_SCOPED_N(active, name)                                                 \
		arc_TRACE_EVENT_SCOPED_NC(active, name, arc_TRACE_COLOR_MAKE(name))

	#define arc_TRACE_EVENT_SCOPED(active)                                                         \
		ZoneNamedC(arc_TRACE_INFO_STORAGE, arc_TRACE_COLOR_MAKE(__FUNCTION__), active)

	/**
	 * Be careful, this macro consist of two statements which are not wrapped in do-while(0)
	 */
	#define arc_TRACE_EVENT_SCOPED_T(active, name)                                                 \
		ZoneTransientNC(arc_TRACE_INFO_STORAGE, name, arc_TRACE_COLOR_MAKE(name), active);

	/** -- FRAME -------------------------------------------------------------------------------- */
	#define arc_TRACE_FRAME_NAMED(active, name)                                                    \
		do                                                                                         \
		{                                                                                          \
			if constexpr (active)                                                                  \
				FrameMarkNamed(name);                                                              \
		} while (0)
	#define arc_TRACE_FRAME(active)                                                                \
		do                                                                                         \
		{                                                                                          \
			if constexpr (active)                                                                  \
				FrameMark;                                                                         \
		} while (0)

	/** -- PLOT --------------------------------------------------------------------------------- */
	/**
	 *  `name` must be a string literal.
	 */
	#define arc_TRACE_PLOT(active, name, value)                                                    \
		do                                                                                         \
		{                                                                                          \
			if constexpr (arc_TRACE_PLOT_ENABLED && active)                                        \
				TracyPlot(name, value);                                                            \
		} while (0)

	/** -- MESSAGE ------------------------------------------------------------------------------ */
	/**
	 *  `message` must be a string literal. Use the -_T version with other strings.
	 */
	#define arc_TRACE_MESSAGE(active, message)                                                     \
		do                                                                                         \
		{                                                                                          \
			if constexpr (active)                                                                  \
				TracyMessageL(message);                                                            \
		} while (0)
	#define arc_TRACE_MESSAGE_T(active, message)                                                   \
		do                                                                                         \
		{                                                                                          \
			if constexpr (active)                                                                  \
			{                                                                                      \
				arc_TRACE_INTERNAL_MESSAGE_T_IMPL(message);                                        \
			}                                                                                      \
		} while (0)

	/** -- LOCKABLE ----------------------------------------------------------------------------- */
	#define arc_TRACE_LOCKABLE(type, varName, desc) TracyLockableN(type, varName, desc)
	#define arc_TRACE_LOCKABLE_TYPE(type) LockableBase(type)
	#define arc_TRACE_LOCKABLE_SHARED(type, varName, desc) TracySharedLockableN(type, varName, desc)
	#define arc_TRACE_LOCKABLE_SHARED_TYPE(type) SharedLockableBase(type)
	#define arc_TRACE_LOCKABLE_RENAME_T(varName, name)                                             \
		do                                                                                         \
		{                                                                                          \
			arc_TRACE_INTERNAL_LOCKABLE_RENAME_T_IMPL(varName, name);                              \
		} while (0)

	/** -- LOCKABLE UTIL ------------------------------------------------------------------------ */
	#define arc_TRACE_CONDITION_VARIABLE std::condition_variable_any
	#define arc_TRACE_CONDITION_VARIABLE_ANY std::condition_variable_any

	/** -- CONTAINERS --------------------------------------------------------------------------- */
	#define arc_TRACE_CONTAINER_UNORDERED_MAP(Key, T) arc_TRACE_unordered_map<Key, T>
	#define arc_TRACE_CONTAINER_QUEUE(T) arc_TRACE_queue<T>
	#define arc_TRACE_CONTAINER_STACK(T) arc_TRACE_stack<T>
	#define arc_TRACE_CONTAINER_VECTOR(T) arc_TRACE_vector<T>
	#define arc_TRACE_CONTAINER_CONFIGURE(varName, name) varName.Configure(name)

	/** -- SOURCE LOCATION UTILS ---------------------------------------------------------------- */
	#define arc_WITH_SOURCE_LOCATION 1

#else

	/** -- EVENT -------------------------------------------------------------------------------- */
	#define arc_TRACE_EVENT_SCOPED_NC(...)
	#define arc_TRACE_EVENT_SCOPED_N(...)
	#define arc_TRACE_EVENT_SCOPED(...)
	#define arc_TRACE_EVENT_SCOPED_T(...)

/** -- ZONE (C API wrappers) ---------------------------------------------------------------- */
using TracyCZoneCtx = const void *;
inline TracyCZoneCtx arc_TRACE_ZONE_BEGIN(std::string_view) noexcept { return nullptr; }
inline void arc_TRACE_ZONE_END(TracyCZoneCtx) noexcept {}

class arc_TRACE_ZONE_SCOPE
{
public:
	arc_TRACE_ZONE_SCOPE(const arc::detail::zone_info &, bool) noexcept {}

	arc_NON_COPYABLE_NON_MOVABLE(arc_TRACE_ZONE_SCOPE);

	~arc_TRACE_ZONE_SCOPE() = default;
};

	/** -- FRAME -------------------------------------------------------------------------------- */
	#define arc_TRACE_FRAME_NAMED(...)
	#define arc_TRACE_FRAME(...)

	/** -- PLOT --------------------------------------------------------------------------------- */
	#define arc_TRACE_PLOT(...)

	/** -- MESSAGE ------------------------------------------------------------------------------ */
	#define arc_TRACE_MESSAGE(...)
	#define arc_TRACE_MESSAGE_T(...)

	/** -- LOCKABLE ----------------------------------------------------------------------------- */
	#define arc_TRACE_LOCKABLE(type, varName, desc) type varName
	#define arc_TRACE_LOCKABLE_TYPE(type) type
	#define arc_TRACE_LOCKABLE_SHARED(type, varName, desc) type varName
	#define arc_TRACE_LOCKABLE_SHARED_TYPE(type) type
	#define arc_TRACE_LOCKABLE_RENAME_T(...)

	/** -- LOCKABLE UTIL ------------------------------------------------------------------------ */
	#define arc_TRACE_CONDITION_VARIABLE std::condition_variable
	#define arc_TRACE_CONDITION_VARIABLE_ANY std::condition_variable_any

	/** -- CONTAINERS --------------------------------------------------------------------------- */
	#define arc_TRACE_CONTAINER_UNORDERED_MAP(Key, T) std::unordered_map<Key, T>
	#define arc_TRACE_CONTAINER_QUEUE(T) std::queue<T>
	#define arc_TRACE_CONTAINER_STACK(T) std::stack<T>
	#define arc_TRACE_CONTAINER_VECTOR(T) std::vector<T>
	#define arc_TRACE_CONTAINER_CONFIGURE(...)
class arc_TRACE_CONTAINER_BASE
{
public:
	void Configure(const char * name_) {}
	void Plot(int64_t value) {}
};

	/** -- SOURCE LOCATION UTILS ---------------------------------------------------------------- */
	#define arc_WITH_SOURCE_LOCATION 0

#endif
