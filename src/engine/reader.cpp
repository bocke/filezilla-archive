#include "../include/reader.h"

#include "engineprivate.h"

#include <libfilezilla/local_filesys.hpp>

reader_factory::reader_factory(std::wstring const& name)
	: name_(name)
{}

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

reader_factory_holder::reader_factory_holder(std::unique_ptr<reader_factory> const& factory)
	: impl_(factory ? factory->clone() : nullptr)
{
}

reader_factory_holder::reader_factory_holder(reader_factory const& factory)
	: impl_(factory.clone())
{
}

reader_factory_holder& reader_factory_holder::operator=(std::unique_ptr<reader_factory> && factory)
{
	if (impl_ != factory) {
		impl_ = std::move(factory);
	}

	return *this;
}

file_reader_factory::file_reader_factory(std::wstring const& file)
	: reader_factory(file)
{
}

std::unique_ptr<reader_factory> file_reader_factory::clone() const
{
	return std::make_unique<file_reader_factory>(*this);
}

uint64_t file_reader_factory::size() const
{
	if (size_) {
		return *size_;
	}
	auto s = fz::local_filesys::get_size(fz::to_native(name_));
	if (s < 0) {
		size_ = aio_base::nosize;
	}
	else {
		size_ = static_cast<uint64_t>(s);
	}
	return *size_;
}

std::unique_ptr<reader_base> file_reader_factory::open(uint64_t offset, CFileZillaEnginePrivate & engine, fz::event_handler & handler, aio_base::shm_flag shm)
{
	auto ret = std::make_unique<file_reader>(name_, engine, handler);

	if (ret->open(offset, shm) != aio_result::ok) {
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

reader_base::reader_base(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler)
	: aio_base(name, engine, handler)
{}

void reader_base::close()
{
	ready_count_ = 0;
	
	remove_reader_events(&handler_, this);
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


file_reader::file_reader(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler)
	: reader_base(name, engine, handler)
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

aio_result file_reader::open(uint64_t offset, shm_flag shm)
{
	if (!allocate_memory(shm)) {
		return aio_result::error;
	}

	if (!file_.open(fz::to_native(name_), fz::file::reading, fz::file::existing)) {
		return aio_result::error;
	}

	start_offset_ = offset;

	if (offset) {
		auto const ofs = static_cast<int64_t>(offset);
		if (file_.seek(ofs, fz::file::begin) != ofs) {
			return aio_result::error;
		}
	}

	thread_ = engine_.GetThreadPool().spawn([this]() { entry(); });
	if (!thread_) {
		return aio_result::error;
	}

	return aio_result::ok;
}

aio_result file_reader::rewind()
{
	if (error_) {
		return aio_result::error;
	}

	auto const ofs = static_cast<int64_t>(start_offset_);
	if (file_.seek(ofs, fz::file::begin) != ofs) {
		error_ = true;
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
			handler_.send_event<read_ready_event>(this);
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
		size_ = nosize;
	}
	else {
		size_ = static_cast<uint64_t>(s);
	}
	return *size_;
}



#include <libfilezilla/buffer.hpp>

memory_reader_factory::memory_reader_factory(std::wstring const& name, fz::buffer & data)
	: reader_factory(name)
	, data_(reinterpret_cast<char const*>(data.get()), data.size())
{}

memory_reader_factory::memory_reader_factory(std::wstring const& name, std::string_view const& data)
	: reader_factory(name)
	, data_(data)
{}

std::unique_ptr<reader_base> memory_reader_factory::open(uint64_t offset, CFileZillaEnginePrivate & engine, fz::event_handler & handler, aio_base::shm_flag shm)
{
	auto ret = std::make_unique<memory_reader>(name_, engine, handler, data_);
	if (ret->open(offset, shm) != aio_result::ok) {
		ret.reset();
	}

	return ret;
}

std::unique_ptr<reader_factory> memory_reader_factory::clone() const
{
	return std::make_unique<memory_reader_factory>(*this);
}

uint64_t memory_reader_factory::size() const
{
	return data_.size();
}


memory_reader::memory_reader(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler, std::string_view const& data)
	: reader_base(name, engine, handler)
	, start_data_(data)
	, data_(start_data_)
{
	ready_count_ = buffer_count;
}

aio_result memory_reader::open(uint64_t offset, shm_flag shm)
{
	if (!allocate_memory(shm)) {
		return aio_result::error;
	}

	if (offset > data_.size()) {
		return aio_result::error;
	}

	data_ = data_.substr(offset);
	start_data_ = data_;

	processing_ = true;
	ready_count_ = 8;
	
	return aio_result::ok;
}


void memory_reader::signal_capacity(fz::scoped_lock & l)
{
	++ready_count_;
	
	size_t c = std::min(data_.size(), buffer_size_);

	auto& b = buffers_[ready_pos_];
	b.resize(c);
	if (c) {
		memcpy(b.get(c), data_.data(), c);
		b.add(c);
		data_ = data_.substr(c);
	}
}

uint64_t memory_reader::size() const
{
	return start_data_.size();
}

aio_result memory_reader::rewind()
{
	data_ = start_data_;
	return aio_result::ok;
}




string_reader::string_reader(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler, std::string const& data)
	: reader_base(name, engine, handler)
	, start_data_(data)
	, data_(start_data_)
{
	ready_count_ = buffer_count;
	processing_ = true;
}

string_reader::string_reader(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler, std::string && data)
	: reader_base(name, engine, handler)
	, start_data_(std::move(data_))
	, data_(start_data_)
{
	ready_count_ = buffer_count;
	processing_ = true;
}

std::unique_ptr<string_reader> string_reader::create(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler, std::string const& data, shm_flag shm)
{
	std::unique_ptr<string_reader> ret(new string_reader(name, engine, handler, data));
	if (!ret->allocate_memory(shm)) {
		ret.reset();
	}

	return ret;
}

std::unique_ptr<string_reader> string_reader::create(std::wstring const& name, CFileZillaEnginePrivate & engine, fz::event_handler & handler, std::string && data, shm_flag shm)
{
	std::unique_ptr<string_reader> ret(new string_reader(name, engine, handler, data));
	if (!ret->allocate_memory(shm)) {
		ret.reset();
	}

	return ret;
}

void string_reader::signal_capacity(fz::scoped_lock & l)
{
	++ready_count_;
	
	size_t c = std::min(data_.size(), buffer_size_);

	auto& b = buffers_[ready_pos_];
	b.resize(c);
	if (c) {
		memcpy(b.get(c), data_.data(), c);
		b.add(c);
		data_ = data_.substr(c);
	}
}

uint64_t string_reader::size() const
{
	return start_data_.size();
}

aio_result string_reader::rewind()
{
	data_ = start_data_;
	return aio_result::ok;
}
