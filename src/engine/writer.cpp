#include "writer.h"

#include <libfilezilla/local_filesys.hpp>

writer_factory_holder::writer_factory_holder(writer_factory_holder const& op)
{
	if (op.impl_) {
		impl_ = std::move(op.impl_->clone());
	}
}

writer_factory_holder& writer_factory_holder::operator=(writer_factory_holder const& op)
{
	if (this != &op && op.impl_) {
		impl_ = std::move(op.impl_->clone());
	}
	return *this;
}

writer_factory_holder::writer_factory_holder(writer_factory_holder && op) noexcept
{
	impl_ = std::move(op.impl_);
	op.impl_.reset();
}

writer_factory_holder& writer_factory_holder::operator=(writer_factory_holder && op) noexcept
{
	if (this != &op) {
		impl_ = std::move(op.impl_);
		op.impl_.reset();
	}

	return *this;
}

writer_factory_holder::writer_factory_holder(std::unique_ptr<writer_factory> && factory)
	: impl_(std::move(factory))
{
}


writer_factory_holder& writer_factory_holder::operator=(std::unique_ptr<writer_factory> && factory)
{
	if (impl_ != factory) {
		impl_ = std::move(factory);
	}

	return *this;
}

file_writer_factory::file_writer_factory(std::wstring const& file, fz::thread_pool & pool)
	: file_(file)
	, pool_(&pool)
{
}

std::unique_ptr<writer_factory> file_writer_factory::clone()
{
	return std::make_unique<file_writer_factory>(*this);
}

uint64_t file_writer_factory::size() const
{
	if (size_) {
		return *size_;
	}
	auto s = fz::local_filesys::get_size(fz::to_native(file_));
	if (s < 0) {
		size_ = npos;
	}
	else {
		size_ = static_cast<uint64_t>(s);
	}
	return *size_;
}

std::unique_ptr<writer_base> file_writer_factory::open(uint64_t offset, fz::event_handler & handler, bool use_shared_memory)
{
	auto ret = std::make_unique<file_writer>(file_);

	if (ret->open(offset, *pool_, handler, use_shared_memory) != aio_result::ok) {
		ret.reset();
	}

	return ret;
}

namespace {
void remove_writer_events(fz::event_handler * handler, writer_base const* writer)
{
	if (!handler) {
		return;
	}
	auto event_filter = [&](fz::event_loop::Events::value_type const& ev) -> bool {
		if (ev.first != handler) {
			return false;
		}
		else if (ev.second->derived_type() == write_ready_event::type()) {
			return std::get<0>(static_cast<write_ready_event const&>(*ev.second).v_) == writer;
		}
		return false;
	};

	handler->event_loop_.filter_events(event_filter);
}
}

void writer_base::close()
{
	ready_count_ = 0;
	
	remove_writer_events(handler_, this);
	handler_ = nullptr;
}

get_write_buffer_result writer_base::get_write_buffer(fz::nonowning_buffer & last_written)
{
	fz::scoped_lock l(mtx_);
	if (error_) {
		return {aio_result::error, fz::nonowning_buffer()};
	}

	if (processing_ && last_written) {
		buffers_[(ready_pos_ + ready_count_) % buffers_.size()] = last_written;
		if (!ready_count_) {
			signal_capacity(l);
		}
		++ready_count_;
	}
	last_written.reset();
	if (ready_count_ >= buffers_.size()) {
		handler_waiting_ = true;
		processing_ = false;
		return {aio_result::wait, fz::nonowning_buffer()};
	}
	else {
		processing_ = true;
		auto b = buffers_[(ready_pos_ + ready_count_) % buffers_.size()];
		b.resize(0);
		return {aio_result::ok, b};
	}
}


file_writer::file_writer(std::wstring const& name)
	: name_(name)
{
}

file_writer::~file_writer()
{
	close();
}

void file_writer::close()
{
	{
		fz::scoped_lock l(mtx_);
		quit_ = true;
		cond_.signal(l);
	}

	thread_.join();

	writer_base::close();
}

aio_result file_writer::open(uint64_t offset, fz::thread_pool & pool, fz::event_handler & handler, bool use_shared_memory)
{
	if (!allocate_memory(use_shared_memory)) {
		return aio_result::error;
	}

	// TODO: Create local dir
	handler_ = &handler;
	if (!file_.open(name_, fz::file::writing, offset ? fz::file::existing : fz::file::empty)) {
		return aio_result::error;
	}

	if (offset) {
		auto const ofs = static_cast<int64_t>(offset);
		if (file_.seek(ofs, fz::file::begin) != ofs) {
			return aio_result::error;
		}
		if (!file_.truncate()) {
			return aio_result::error;
		}
	}

	thread_ = pool.spawn([this]() { entry(); });
	if (!thread_) {
		return aio_result::error;
	}

	return aio_result::ok;
}

void file_writer::entry()
{
	fz::scoped_lock l(mtx_);
	while (!quit_ && !error_) {
		if (!ready_count_) {
			if (handler_waiting_) {
				handler_waiting_ = false;
				handler_->send_event<write_ready_event>(this);
				break;
			}

			cond_.wait(l);
			continue;
		}

		fz::nonowning_buffer & b = buffers_[ready_pos_];

		while (!b.empty()) {
			l.unlock();
			auto written = file_.write(b.get(), b.size());
			l.lock();
			if (quit_) {
				return;
			}
			if (written > 0) {
				b.consume(static_cast<size_t>(written));
			}
			else {
				error_ = true;
				break;
			}
		}

		ready_pos_ = (ready_pos_ + 1) % buffers_.size();
		--ready_count_;

		if (handler_waiting_) {
			handler_waiting_ = false;
			handler_->send_event<write_ready_event>(this);
		}
	}
}

void file_writer::signal_capacity(fz::scoped_lock & l)
{
	cond_.signal(l);
}

uint64_t file_writer::size() const
{
	fz::scoped_lock l(mtx_);
	if (size_) {
		return *size_;
	}
	auto s = file_.size();
	if (s < 0) {
		size_ = npos;
	}
	else {
		size_ = static_cast<uint64_t>(s);
	}
	return *size_;
}

aio_result file_writer::finalize(fz::nonowning_buffer & last_written)
{
	fz::scoped_lock l(mtx_);
	if (error_) {
		return aio_result::error;
	}
	if (processing_ && last_written) {
		buffers_[(ready_pos_ + ready_count_) % buffers_.size()] = last_written;
		last_written.reset();
		processing_ = false;
		if (!ready_count_) {
			signal_capacity(l);
		}
		++ready_count_;
	}
	if (ready_count_) {
		handler_waiting_ = true;
		return aio_result::wait;
	}
	return aio_result::ok;
}
