#pragma once

#include <queue>
#include <stack>
#include <unordered_map>

#if __has_include(<tracy/Tracy.hpp>)
	#define arc_TRACE_INSTRUMENTATION_ENABLE 1
#else
	#undef arc_TRACE_INSTRUMENTATION_ENABLE
#endif

#if arc_TRACE_INSTRUMENTATION_ENABLE

	#include "arc/extra/algorithms.hpp"

	#include <tracy/Tracy.hpp>

	#define arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(functionName, constNoexceptVolatile)         \
		template <typename... Args>                                                                \
		decltype(auto) functionName(Args &&... args) constNoexceptVolatile                         \
		{                                                                                          \
			return container.functionName(std::forward<Args>(args)...);                            \
		}

class arc_TRACE_CONTAINER_BASE
{
public:
	arc_TRACE_CONTAINER_BASE() = default;

	/**
	 * Must be called before fist use of the container.
	 * \p name must come from a string literal.
	 */
	void Configure(const char * name_)
	{
		name = name_;
		TracyPlotConfig(name, tracy::PlotFormatType::Number, true, true, 0);
		TracyPlot(name, int64_t(0));
	}

	~arc_TRACE_CONTAINER_BASE() { TracyPlot(name, int64_t(0)); }

protected:
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
			TracyPlot(name, int64_t(container.size()));
		return result;
	}

	template <typename... Args>
	decltype(auto) emplace(Args &&... args)
	{
		decltype(auto) result = container.emplace(std::forward<Args>(args)...);
		if (result.second)
			TracyPlot(name, int64_t(container.size()));
		return result;
	}

	template <typename... Args>
	decltype(auto) erase(Args &&... args)
	{
		decltype(auto) result = container.erase(std::forward<Args>(args)...);
		TracyPlot(name, int64_t(container.size()));
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
		TracyPlot(name, int64_t(container.size()));
		return result;
	}

	void pop()
	{
		container.pop();
		TracyPlot(name, int64_t(container.size()));
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
		TracyPlot(name, int64_t(container.size()));
		return result;
	}
	void pop()
	{
		container.pop();
		TracyPlot(name, int64_t(container.size()));
	}

	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(top, const);
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(top, );
	arc_TRACE_INTERNAL_DEFINE_PERFECT_FORWARD(size, const);

private:
	wrapped_type container;
};

inline constexpr uint32_t arc_TRACE_COLOR_MAKE(uint8_t r, uint8_t g, uint8_t b)
{
	return uint32_t(r) << uint32_t(16) | uint32_t(g) << uint32_t(8) | uint32_t(b);
}

inline constexpr uint32_t arc_TRACE_COLOR_MAKE(std::string_view str)
{
	uint32_t hash = arc::extra::FNV_1a_32(str);

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
		ZoneTransientN(arc_TRACE_INFO_STORAGE, name, active);                                      \
		arc_TRACE_INFO_STORAGE.Color(arc_TRACE_COLOR_MAKE(name))

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
				std::string_view nameView{ message };                                              \
				TracyMessage(nameView.data(), nameView.size());                                    \
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
			std::string_view nameView{ name };                                                     \
			LockableName(lk, nameView.data(), nameView.size());                                    \
		} while (0)

	/** -- LOCKABLE UTIL ------------------------------------------------------------------------ */
	#define arc_TRACE_CONDITION_VARIABLE std::condition_variable_any
	#define arc_TRACE_CONDITION_VARIABLE_ANY std::condition_variable_any

	/** -- CONTAINERS --------------------------------------------------------------------------- */
	#define arc_TRACE_CONTAINER_UNORDERED_MAP(Key, T) arc_TRACE_unordered_map<Key, T>
	#define arc_TRACE_CONTAINER_QUEUE(T) arc_TRACE_queue<T>
	#define arc_TRACE_CONTAINER_STACK(T) arc_TRACE_stack<T>
	#define arc_TRACE_CONTAINER_CONFIGURE(varName, name) varName.Configure(name)

#else

	#define arc_TRACE_COLOR_MAKE(...)

	/** -- EVENT -------------------------------------------------------------------------------- */
	#define arc_TRACE_EVENT_SCOPED_NC(...)
	#define arc_TRACE_EVENT_SCOPED_N(...)
	#define arc_TRACE_EVENT_SCOPED(...)
	#define arc_TRACE_EVENT_SCOPED_T(...)

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
	#define arc_TRACE_CONTAINER_CONFIGURE(...)

#endif

/** -- GLOBAL SWITCHES -------------------------------------------------------------------------- */
static constexpr bool arc_TRACE_PLOT_ENABLED = true;
static constexpr bool arc_TRACE_CORO = true;
static constexpr bool arc_TRACE_GRAPH = true;
static constexpr bool arc_TRACE_GRAPH_SLEEP = true;
static constexpr bool arc_TRACE_MEMORY = true;
static constexpr bool arc_TRACE_OTHER = true;
static constexpr bool arc_TRACE_RENDER = true;
static constexpr bool arc_TRACE_FILE_IO = true;
