#include "../include/aio.h"

namespace {
size_t get_page_size()
{
	static size_t const page_size = []() { SYSTEM_INFO i{}; GetSystemInfo(&i); return i.dwPageSize; }();
	return page_size;
}
}

aio_base::~aio_base()
{
	if (mapping_ != INVALID_HANDLE_VALUE) {
		UnmapViewOfFile(memory_);
		CloseHandle(mapping_);
	}
	else {
		delete [] memory_;
	}
}

bool aio_base::allocate_memory(bool use_shared_memory)
{
	if (memory_) {
		return true;
	}

	// Since different threads/processes operate on different buffers at the same time, use
	// seperate them with a padding page to prevent false sharing due to automatic prefetching.
	memory_size_ = (buffer_size_ + get_page_size()) * buffer_count + get_page_size();
	if (use_shared_memory) {
		HANDLE mapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(memory_size_), nullptr);
		if (!mapping || mapping == INVALID_HANDLE_VALUE) {
			return false;
		}
		memory_ = static_cast<uint8_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, memory_size_));
		if (!memory_) {
			return false;
		}
		mapping_ = mapping;
	}
	else {
		memory_ = new uint8_t[memory_size_];
	}
	for (size_t i = 0; i < buffer_count; ++i) {
		buffers_[i] = fz::nonowning_buffer(memory_ + i * (buffer_size_ + get_page_size()) + get_page_size(), buffer_size_);
	}

	return true;
}

std::tuple<HANDLE, uint8_t const*, size_t> aio_base::shared_memory_info() const
{
	return std::make_tuple(mapping_, memory_, memory_size_);
}
