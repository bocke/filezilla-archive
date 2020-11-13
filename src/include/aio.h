#ifndef FILEZILLA_ENGINE_AIO_HEADER
#define FILEZILLA_ENGINE_AIO_HEADER

#include "visibility.h"

#include <libfilezilla/nonowning_buffer.hpp>
#include <libfilezilla/mutex.hpp>

#include <array>
#include <tuple>

enum class aio_result
{
	ok,
	wait,
	error
};

class FZC_PUBLIC_SYMBOL aio_base
{
public:
	virtual ~aio_base();

	// The buffer size aimed for, actual buffer size may differ
	static constexpr size_t buffer_size_{256*1024};
	static constexpr size_t buffer_count{8};

#if FZ_WINDOWS
	std::tuple<HANDLE, uint8_t const*, size_t> shared_memory_info() const;
#else
	std::tuple<int, uint8_t const*, size_t> shared_memory_info() const;
#endif

protected:
	mutable fz::mutex mtx_{false};

	bool allocate_memory(bool use_shared_memory);

	std::array<fz::nonowning_buffer, buffer_count> buffers_;
	size_t ready_pos_{};
	size_t ready_count_{};
	bool processing_{};

private:
#if FZ_WINDOWS
	HANDLE mapping_{INVALID_HANDLE_VALUE};
#else
	int mapping_{-1};
#endif
	size_t memory_size_{};
	uint8_t* memory_{};

};

#endif
