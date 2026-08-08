#pragma once

/**
 * Makes a struct or a class non-movable and non-copyable when invoking the macro in the body:
 *
 * struct my_struct
 * {
 *     arc_NON_COPYABLE_NON_MOVABLE(my_struct);
 * };
 */
#define arc_NON_COPYABLE_NON_MOVABLE(type)                                                         \
	type(const type &) = delete;                                                                   \
	type(type &&) = delete;                                                                        \
	type & operator=(const type &) = delete;                                                       \
	type & operator=(type &&) = delete
