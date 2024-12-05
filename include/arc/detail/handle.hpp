#pragma once

#include "arc/fwd.hpp"

struct arc::detail::handle
{
public:
	handle() = default;
	handle(handle && other);
	handle(const handle & other);
	handle & operator=(handle && other);
	handle & operator=(const handle & other);
	~handle();
	handle & operator=(std::nullptr_t);
	explicit operator bool() const noexcept;
	arc::detail::store_entry * operator->() const noexcept;

	bool operator==(const handle & other) const noexcept { return storeEntry == other.storeEntry; }

private:
	explicit handle(arc::detail::store_entry * storeEntry);

	void acquire();
	void release();
	void abandon();

	arc::detail::store_entry * storeEntry = nullptr;

	friend arc::detail::store;
	friend arc::detail::control_block;

	template <typename F, size_t keyCount>
	friend void arc::detail::create_shared_task(arc::detail::store_entry & storeEntry);
};
