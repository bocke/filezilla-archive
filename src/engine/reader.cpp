#include "../include/reader.h"

#include <libfilezilla/local_filesys.hpp>

reader_factory_holder::reader_factory_holder(reader_factory_holder const& op)
{
	if (op.impl_) {
		impl_ = op.impl_->clone();
	}
}

reader_factory_holder& reader_factory_holder::operator=(reader_factory_holder const& op)
{
	if (this != &op && op.impl_) {
		impl_ = op.impl_->clone();
	}
	return *this;
}

reader_factory_holder::reader_factory_holder(reader_factory_holder && op) noexcept
{
	impl_ = std::move(op.impl_);
	op.impl_.reset();
}

reader_factory_holder& reader_factory_holder::operator=(reader_factory_holder && op) noexcept
{
	if (this != &op) {
		impl_ = std::move(op.impl_);
		op.impl_.reset();
	}

	return *this;
}

reader_factory_holder::reader_factory_holder(std::unique_ptr<reader_factory> && factory)
	: impl_(std::move(factory))
{
}


reader_factory_holder& reader_factory_holder::operator=(std::unique_ptr<reader_factory> && factory)
{
	if (impl_ != factory) {
		impl_ = std::move(factory);
	}

	return *this;
}

file_reader_factory::file_reader_factory(std::wstring const& file, fz::thread_pool & pool)
	: file_(file)
	, pool_(&pool)
{
}

std::unique_ptr<reader_factory> file_reader_factory::clone()
{
	return std::make_unique<file_reader_factory>(*this);
}

uint64_t file_reader_factory::size() const
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

std::unique_ptr<reader_base> file_reader_factory::open(uint64_t offset, fz::event_handler & handler, aio_base::shm_flag shm)
{
	auto ret = std::make_unique<file_reader>(file_);

	if (ret->open(offset, *pool_, handler, shm) != aio_result::ok) {
		ret.reset();
	}

	return ret;
}

namespace {
void remove_reader_events(fz::event_handler * handler, reader_base const* reader)
{
	if (!handler) {
		return;
	}
	auto event_filter = [&](fz::event_loop::Events::value_type const& ev) -> bool {
		if (ev.first != handler) {
			return false;
		}
		else if (ev.second->derived_type() == read_ready_event::type()) {
			return std::get<0>(static_cast<read_ready_event const&>(*ev.second).v_) == reader;
		}
		return false;
	};

	handler->event_loop_.filter_events(event_filter);
}
}

void reader_base::close()
{
	ready_count_ = 0;
	
	remove_reader_events(handler_, this);
	handler_ = nullptr;
}

read_result reader_base::read()
{
	fz::scoped_lock l(mtx_);
	if (error_) {
		return {aio_result::error, fz::nonowning_buffer()};
	}

	if (processing_) {
		ready_pos_ = (ready_pos_ + 1) % buffers_.size();
		if (ready_count_ == buffers_.size()) {
			signal_capacity(l);
		}
		--ready_count_;
	}
	if (ready_count_) {
		processing_ = true;
		return {aio_result::ok, buffers_[ready_pos_]};
	}
	else if (error_) {
		return {aio_result::error, fz::nonowning_buffer()};
	}
	else {
		handler_waiting_ = true;
		processing_ = false;
		return {aio_result::wait, fz::nonowning_buffer()};
	}
}


file_reader::file_reader(std::wstring const& name)
	: name_(name)
{
}

file_reader::~file_reader()
{
	close();
}

void file_reader::close()
{
	{
		fz::scoped_lock l(mtx_);
		quit_ = true;
		cond_.signal(l);
	}

	thread_.join();

	reader_base::close();
}

aio_result file_reader::open(uint64_t offset, fz::thread_pool & pool, fz::event_handler & handler, shm_flag shm)
{
	if (!allocate_memory(shm)) {
		return aio_result::error;
	}

	handler_ = &handler;
	if (!file_.open(fz::to_native(name_), fz::file::reading, fz::file::existing)) {
		return aio_result::error;
	}

	if (offset) {
		auto const ofs = static_cast<int64_t>(offset);
		if (file_.seek(ofs, fz::file::begin) != ofs) {
			return aio_result::error;
		}
	}

	thread_ = pool.spawn([this]() { entry(); });
	if (!thread_) {
		return aio_result::error;
	}

	return aio_result::ok;
}

void file_reader::entry()
{
	fz::scoped_lock l(mtx_);
	while (!quit_ && !error_) {
		if (ready_count_ >= buffers_.size()) {
			cond_.wait(l);
			continue;
		}

		fz::nonowning_buffer & b = buffers_[(ready_pos_ + ready_count_) % buffers_.size()];
		b.resize(0);

		l.unlock();
		auto read = file_.read(b.get(b.capacity()), b.capacity());
		l.lock();

		if (quit_) {
			return;
		}

		if (read >= 0) {
			b.add(read);
			++ready_count_;
		}
		else {
			error_ = true;
		}

		if (handler_waiting_) {
			handler_waiting_ = false;
			handler_->send_event<read_ready_event>(this);
		}

		if (read <= 0) {
			break;
		}
	}
}

void file_reader::signal_capacity(fz::scoped_lock & l)
{
	cond_.signal(l);
}

uint64_t file_reader::size() const
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
