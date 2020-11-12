#ifndef FILEZILLA_ENGINE_WRITER_HEADER
#define FILEZILLA_ENGINE_WRITER_HEADER

#include "aio.h"

#include <libfilezilla/event.hpp>
#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/file.hpp>
#include <libfilezilla/thread_pool.hpp>

#include <optional>

class writer_base;

struct write_ready_event_type{};
typedef fz::simple_event<write_ready_event_type, writer_base*> write_ready_event;

class writer_base;

class FZC_PUBLIC_SYMBOL writer_factory
{
public:
	virtual ~writer_factory() noexcept = default;

	static constexpr auto npos = static_cast<uint64_t>(-1);

	virtual std::unique_ptr<writer_factory> clone() = 0;

	virtual std::unique_ptr<writer_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory) = 0;
	virtual uint64_t size() const { return static_cast<uint64_t>(-1); }
protected:
	writer_factory() = default;
	writer_factory(writer_factory const&) = default;
	writer_factory& operator=(writer_factory const&) = default;
};

class FZC_PUBLIC_SYMBOL writer_factory_holder final
{
public:
	static constexpr auto npos = static_cast<uint64_t>(-1);

	writer_factory_holder() = default;
	explicit writer_factory_holder(std::unique_ptr<writer_factory> && factory);

	writer_factory_holder(writer_factory_holder const& op);
	writer_factory_holder& operator=(writer_factory_holder const& op);

	writer_factory_holder(writer_factory_holder && op) noexcept;
	writer_factory_holder& operator=(writer_factory_holder && op) noexcept;
	writer_factory_holder& operator=(std::unique_ptr<writer_factory> && factory);

	std::unique_ptr<writer_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory)
	{
		return impl_ ? impl_->open(offset, handler, use_shared_memory) : nullptr;
	}

	uint64_t size() const
	{
		return impl_ ? impl_->size() : static_cast<uint64_t>(-1);
	}

	explicit operator bool() const { return impl_.operator bool(); }

private:
	std::unique_ptr<writer_factory> impl_;
};

class FZC_PUBLIC_SYMBOL file_writer_factory final : public writer_factory
{
public:
	file_writer_factory(std::wstring const& file, fz::thread_pool & pool);
	
	virtual std::unique_ptr<writer_base> open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory) override;
	virtual std::unique_ptr<writer_factory> clone() override;

	virtual uint64_t size() const override;

	std::wstring file_;
	mutable std::optional<uint64_t> size_;

	fz::thread_pool * pool_{};
};


struct get_write_buffer_result {
	bool operator==(aio_result const t) const { return type_ == t; }

	aio_result type_{aio_result::error};
	fz::nonowning_buffer buffer_;
};

class FZC_PUBLIC_SYMBOL writer_base : public aio_base
{
public:
	static constexpr auto npos = static_cast<uint64_t>(-1);

	virtual void close();

	virtual aio_result finalize(fz::nonowning_buffer & last_written) = 0;

	// May be empty
	virtual std::wstring const& name() const = 0;

	virtual uint64_t size() const { return static_cast<uint64_t>(-1); }

	get_write_buffer_result get_write_buffer(fz::nonowning_buffer & last_written);

	// The buffer size aimed for, actual buffer size may differ
	static constexpr size_t buffer_size{256*1024};

protected:
	virtual void signal_capacity(fz::scoped_lock & l) = 0;

	fz::event_handler * handler_{};

	bool quit_{};
	bool error_{};
	bool handler_waiting_{};
};

class FZC_PUBLIC_SYMBOL file_writer final : public writer_base
{
public:
	explicit file_writer(std::wstring const& name);
	~file_writer();

	virtual void close() override;

	virtual aio_result finalize(fz::nonowning_buffer & last_written) override;

	virtual std::wstring const& name() const override { return name_; }
	virtual uint64_t size() const override;

protected:
	virtual void signal_capacity(fz::scoped_lock & l) override;

private:
	friend class file_writer_factory;
	aio_result open(uint64_t offset, fz::thread_pool & pool_, fz::event_handler & handler, bool use_shared_memory);

	void entry();

	fz::file file_;

	mutable std::optional<uint64_t> size_;

	fz::async_task thread_;
	fz::condition cond_;

	std::wstring const name_;
};

#endif
