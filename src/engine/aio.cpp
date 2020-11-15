#include "../include/aio.h"

#ifndef FZ_WINDOWS
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
size_t get_page_size()
{
#if FZ_WINDOWS
	static size_t const page_size = []() { SYSTEM_INFO i{}; GetSystemInfo(&i); return i.dwPageSize; }();
#else
	static size_t const page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
	return page_size;
}
}

aio_base::~aio_base()
{
#if FZ_WINDOWS
	if (mapping_ != INVALID_HANDLE_VALUE) {
		UnmapViewOfFile(memory_);
		CloseHandle(mapping_);
	}
#else
	if (mapping_ != -1) {
		munmap(memory_, memory_size_);
	}
#endif
	else {
		delete [] memory_;
	}
}

bool aio_base::allocate_memory(shm_flag shm)
{
	if (memory_) {
		return true;
	}

	// Since different threads/processes operate on different buffers at the same time, use
	// seperate them with a padding page to prevent false sharing due to automatic prefetching.
	memory_size_ = (buffer_size_ + get_page_size()) * buffer_count + get_page_size();
#if FZ_WINDOWS
	if (shm) {
		HANDLE mapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(memory_size_), nullptr);
		if (!mapping || mapping == INVALID_HANDLE_VALUE) {
			return false;
		}
		memory_ = static_cast<uint8_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, memory_size_));
		if (!memory_) {
			return false;
		}
		mapping_ = mapping;
#else
	if (shm >= 0) {
		if (ftruncate(shm, memory_size_) != 0) {
			return false;
		}
		memory_ = static_cast<uint8_t*>(mmap(nullptr, memory_size_, PROT_READ|PROT_WRITE, MAP_SHARED, shm, 0));
		if (!memory_) {
			return false;
		}
		mapping_ = shm;
#endif
	}
	else {
		memory_ = new uint8_t[memory_size_];
	}
	for (size_t i = 0; i < buffer_count; ++i) {
		buffers_[i] = fz::nonowning_buffer(memory_ + i * (buffer_size_ + get_page_size()) + get_page_size(), buffer_size_);
	}

	return true;
}

std::tuple<aio_base::shm_handle, uint8_t const*, size_t> aio_base::shared_memory_info() const
{
	return std::make_tuple(mapping_, memory_, memory_size_);
}
