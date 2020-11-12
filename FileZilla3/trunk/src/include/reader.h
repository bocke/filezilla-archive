#ifndef FILEZILLA_ENGINE_IO_HEADER
#define FILEZILLA_ENGINE_IO_HEADER

#include "aio.h"

#include <libfilezilla/event.hpp>
#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/file.hpp>
#include <libfilezilla/thread_pool.hpp>

#include <optional>

class reader_base;

struct read_ready_event_type{};
typedef fz::simple_event<read_ready_event_type, reader_base*> read_ready_event;

class FZC_PUBLIC_SYMBOL reader_factory
{
public:
	virtual ~reader_factory() noexcept = default;

	static constexpr auto npos = static_cast<uint64_t>(-1);

	virtual std::unique_ptr<reader_factory> clone() = 0;

	// If use_shared_memory is true, the buffers are allocated in shared memory suitable for communication with child processes.
	virtual std::unique_ptr<reader_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory) = 0;

	virtual uint64_t size() const { return static_cast<uint64_t>(-1); }

protected:
	reader_factory() = default;
	reader_factory(reader_factory const&) = default;
	reader_factory& operator=(reader_factory const&) = default;
};

class FZC_PUBLIC_SYMBOL reader_factory_holder final
{
public:
	static constexpr auto npos = static_cast<uint64_t>(-1);

	reader_factory_holder() = default;
	explicit reader_factory_holder(std::unique_ptr<reader_factory> && factory);

	reader_factory_holder(reader_factory_holder const& op);
	reader_factory_holder& operator=(reader_factory_holder const& op);

	reader_factory_holder(reader_factory_holder && op) noexcept;
	reader_factory_holder& operator=(reader_factory_holder && op) noexcept;
	reader_factory_holder& operator=(std::unique_ptr<reader_factory> && factory);

	std::unique_ptr<reader_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory)
	{
		return impl_ ? impl_->open(offset, handler, use_shared_memory) : nullptr;
	}

	uint64_t size() const
	{
		return impl_ ? impl_->size() : static_cast<uint64_t>(-1);
	}

	explicit operator bool() const { return impl_.operator bool(); }

private:
	std::unique_ptr<reader_factory> impl_;
};

class FZC_PUBLIC_SYMBOL file_reader_factory final : public reader_factory
{
public:
	file_reader_factory(std::wstring const& file, fz::thread_pool & pool);
	
	virtual std::unique_ptr<reader_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory) override;
	virtual std::unique_ptr<reader_factory> clone() override;

	virtual uint64_t size() const override;

	std::wstring file_;
	mutable std::optional<uint64_t> size_;

	fz::thread_pool * pool_{};
};

struct read_result {
	bool operator==(aio_result const t) const { return type_ == t; }

	aio_result type_{aio_result::error};

	// If type is ok and buffer is empty, we're at eof
	fz::nonowning_buffer buffer_;
};

class FZC_PUBLIC_SYMBOL reader_base : public aio_base
{
public:
	static constexpr auto npos = static_cast<uint64_t>(-1);

	virtual void close();

	// May be empty
	virtual std::wstring const& name() const = 0;

	virtual uint64_t size() const { return static_cast<uint64_t>(-1); }

	read_result read();

protected:
	virtual void signal_capacity(fz::scoped_lock & l) = 0;

	fz::event_handler * handler_{};

	bool quit_{};
	bool error_{};
	bool handler_waiting_{};
};

class FZC_PUBLIC_SYMBOL file_reader final : public reader_base
{
public:
	explicit file_reader(std::wstring const& name);
	~file_reader();

	virtual void close() override;

	virtual std::wstring const& name() const override { return name_; }
	virtual uint64_t size() const override;

protected:
	virtual void signal_capacity(fz::scoped_lock & l) override;

private:
	friend class file_reader_factory;
	aio_result open(uint64_t offset, fz::thread_pool & pool_, fz::event_handler & handler, bool use_shared_memory);

	void entry();

	fz::file file_;

	mutable std::optional<uint64_t> size_;

	fz::async_task thread_;
	fz::condition cond_;

	std::wstring const name_;
};

#endif
