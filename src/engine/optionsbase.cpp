#include "../include/optionsbase.h"

#include <libfilezilla/event_handler.hpp>

#ifdef HAVE_LIBPUGIXML
#include <pugixml.hpp>
#else
#include "../pugixml/pugixml.hpp"
#endif

fz::mutex COptionsBase::mtx_{false};
std::vector<option_def> COptionsBase::options_;
std::map<std::string, size_t, std::less<>> COptionsBase::name_to_option_;

option_def::option_def(std::string_view name, std::wstring_view def, option_flags flags, size_t max_len)
	: name_(name)
	, default_(def)
	, type_(option_type::string)
	, flags_(flags)
	, max_(static_cast<int>(max_len))
{}

option_def::option_def(std::string_view name, std::wstring_view def, option_flags flags, option_type t, size_t max_len, bool (*validator)(std::wstring& v))
	: name_(name)
	, default_(def)
	, type_(t)
	, flags_(flags)
	, max_(static_cast<int>(max_len))
	, validator_((t == option_type::string) ? reinterpret_cast<void*>(validator) : nullptr)
{
}

option_def::option_def(std::string_view name, std::wstring_view def, option_flags flags, bool (*validator)(pugi::xml_node&))
	: name_(name)
	, default_(def)
	, type_(option_type::xml)
	, flags_(flags)
	, max_(10000000)
	, validator_(reinterpret_cast<void*>(validator))
{}

option_def::option_def(std::string_view name, int def, option_flags flags, int min, int max, bool (*validator)(int& v))
	: name_(name)
	, default_(fz::to_wstring(def))
	, type_(option_type::number)
	, flags_(flags)
	, min_(min)
	, max_(max)
	, validator_(reinterpret_cast<void*>(validator))
{}

template<>
option_def::option_def(std::string_view name, bool def, option_flags flags)
	: name_(name)
	, default_(fz::to_wstring(def))
	, type_(option_type::boolean)
	, flags_(flags)
	, max_(1)
{}



namespace {
void event_handler_option_watcher_notifier(void* handler, watched_options&& options)
{
	static_cast<fz::event_handler*>(handler)->send_event<options_changed_event>(std::move(options));
}
}

std::tuple<void*, watcher_notifier> get_option_watcher_notifier(fz::event_handler * handler)
{
	return std::make_tuple(handler, &event_handler_option_watcher_notifier);
}


unsigned int COptionsBase::register_options(std::initializer_list<option_def> options)
{
	fz::scoped_lock l_(mtx_);
	size_t const prev = options_.size();
	options_.insert(options_.end(), options);
	for (size_t i = prev; i < options_.size(); ++i) {
		name_to_option_[options_[i].name()] = i;
	}

	return static_cast<unsigned int>(prev);
}


void COptionsBase::add_missing()
{
	if (values_.size() >= options_.size()) {
		return;
	}

	size_t i = values_.size();
	values_.resize(options_.size());

	for (; i < options_.size(); ++i) {
		auto& val = values_[i];
		auto const& def = options_[i];

		if (def.type() == option_type::xml) {
			val.xml_ = std::make_unique<pugi::xml_document>();
			val.xml_->load_string(fz::to_utf8(def.def()).c_str());
		}
		else {
			val.str_ = def.def();
			val.v_ = fz::to_integral<int>(def.def());
		}
	}
}


int COptionsBase::get_int(optionsIndex opt)
{
	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return 0;
	}

	add_missing();
	auto& val = values_[static_cast<size_t>(opt)];
	return val.v_;
}

std::wstring COptionsBase::get_string(optionsIndex opt)
{
	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return std::wstring();
	}

	add_missing();
	auto& val = values_[static_cast<size_t>(opt)];
	return val.str_;
}

pugi::xml_document COptionsBase::get_xml(optionsIndex opt)
{
	pugi::xml_document ret;

	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return ret;
	}

	add_missing();
	auto& val = values_[static_cast<size_t>(opt)];
	for (auto c = val.xml_->first_child(); c; c = c.next_sibling()) {
		ret.append_copy(c);
	}
	return ret;
}

bool COptionsBase::from_default(optionsIndex opt)
{
	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return false;
	}

	add_missing();
	auto& val = values_[static_cast<size_t>(opt)];
	return val.from_default_;
}

void COptionsBase::set(optionsIndex opt, int value)
{
	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return;
	}

	add_missing();
	auto const& def = options_[static_cast<size_t>(opt)];
	auto& val = values_[static_cast<size_t>(opt)];

	// Type conversion
	if (def.type() == option_type::number) {
		set(opt, def, val, value);
	}
	else if (def.type() == option_type::boolean) {
		set(opt, def, val, value ? 1 : 0);
	}
	else if (def.type() == option_type::string) {
		set(opt, def, val, fz::to_wstring(value));
	}
}

void COptionsBase::set(optionsIndex opt, std::wstring_view const& value, bool from_default)
{
	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return;
	}

	add_missing();
	auto const& def = options_[static_cast<size_t>(opt)];
	auto& val = values_[static_cast<size_t>(opt)];

	// Type conversion
	if (def.type() == option_type::number) {
		set(opt, def, val, fz::to_integral<int>(value), from_default);
	}
	else if (def.type() == option_type::boolean) {
		set(opt, def, val, fz::to_integral<int>(value), from_default);
	}
	else if (def.type() == option_type::string) {
		set(opt, def, val, value, from_default);
	}
}

void COptionsBase::set(optionsIndex opt, pugi::xml_node const& value)
{
	pugi::xml_document doc;
	if (value) {
		if (value.type() == pugi::node_document) {
			for (auto c = value.first_child(); c; c = c.next_sibling()) {
				if (c.type() == pugi::node_element) {
					doc.append_copy(c);
				}
			}
		}
		else {
			doc.append_copy(value);
		}
	}

	fz::scoped_lock l(mtx_);
	if (opt == optionsIndex::invalid || static_cast<size_t>(opt) >= options_.size()) {
		return;
	}

	add_missing();
	auto const& def = options_[static_cast<size_t>(opt)];
	auto& val = values_[static_cast<size_t>(opt)];

	// Type check
	if (def.type() != option_type::xml) {
		return;
	}

	set(opt, def, val, std::move(doc));
}

void COptionsBase::set(optionsIndex opt, option_def const& def, option_value& val, int value, bool from_default)
{
	if ((def.flags() & option_flags::default_only) && !from_default) {
		return;
	}
	if ((def.flags() & option_flags::default_priority) && !from_default && val.from_default_) {
		return;
	}

	if (value < def.min()) {
		if (!(def.flags() & option_flags::numeric_clamp)) {
			return;
		}
		value = def.min();
	}
	else if (value > def.max()) {
		if (!(def.flags() & option_flags::numeric_clamp)) {
			return;
		}
		value = def.max();
	}
	if (def.validator()) {
		if (!reinterpret_cast<bool(*)(int&)>(def.validator())(value)) {
			return;
		}
	}

	val.from_default_ = from_default;
	if (value == val.v_) {
		return;
	}

	val.v_ = value;
	val.str_ = fz::to_wstring(value);

	set_changed(opt);
}

void COptionsBase::set(optionsIndex opt, option_def const& def, option_value& val, std::wstring_view const& value, bool from_default)
{
	if ((def.flags() & option_flags::default_only) && !from_default) {
		return;
	}
	if ((def.flags() & option_flags::default_priority) && !from_default && val.from_default_) {
		return;
	}

	if (value.size() > static_cast<size_t>(def.max())) {
		return;
	}

	if (def.validator()) {
		std::wstring v(value);
		if (!reinterpret_cast<bool(*)(std::wstring&)>(def.validator())(v)) {
			return;
		}

		val.from_default_ = from_default;
		if (v == val.str_) {
			return;
		}
		val.v_ = fz::to_integral<int>(v);
		val.str_ = std::move(v);
	}
	else {

		val.from_default_ = from_default;
		if (value == val.str_) {
			return;
		}
		val.v_ = fz::to_integral<int>(value);
		val.str_ = value;
	}

	set_changed(opt);
}

void COptionsBase::set(optionsIndex opt, option_def const& def, option_value& val, pugi::xml_document&& value, bool from_default)
{
	if ((def.flags() & option_flags::default_only) && !from_default) {
		return;
	}
	if ((def.flags() & option_flags::default_priority) && !from_default && val.from_default_) {
		return;
	}

	if (def.validator()) {
		if (!reinterpret_cast<bool(*)(pugi::xml_node&)>(def.validator())(value)) {
			return;
		}
	}
	*val.xml_ = std::move(value);

	set_changed(opt);
}

void COptionsBase::set_changed(optionsIndex opt)
{
	bool notify = can_notify_ && !changed_.any();
	changed_.set(opt);
	if (notify) {
		notify_changed();
	}
}

bool watched_options::any() const
{
	for (auto const& v : options_) {
		if (v) {
			return true;
		}
	}

	return false;
}

void watched_options::set(optionsIndex opt)
{
	auto idx = static_cast<size_t>(opt) / 64;
	if (idx >= options_.size()) {
		options_.resize(idx + 1);
	}

	options_[idx] |= (1ull << static_cast<size_t>(opt) % 64);
}

void watched_options::unset(optionsIndex opt)
{
	auto idx = static_cast<size_t>(opt) / 64;
	if (idx < options_.size()) {
		options_[idx] &= ~(1ull << static_cast<size_t>(opt) % 64);
	}

}

bool watched_options::test(optionsIndex opt) const
{
	auto idx = static_cast<size_t>(opt) / 64;
	if (idx >= options_.size()) {
		return false;
	}

	return options_[idx] & (1ull << static_cast<size_t>(opt) % 64);
}

watched_options& watched_options::operator&=(std::vector<uint64_t> const& op)
{
	size_t s = std::min(options_.size(), op.size());
	options_.resize(s);
	for (size_t i = 0; i < s; ++i) {
		options_[i] &= op[i];
	}
	return *this;
}

void COptionsBase::continue_notify_changed()
{
	watched_options changed;
	{
		fz::scoped_lock l(mtx_);
		if (!changed_) {
			return;
		}
		changed = changed_;
		changed_.clear();
		process_changed(changed);
	}

	fz::scoped_lock l(notification_mtx_);

	for (auto const& w : watchers_) {
		watched_options n = changed;
		if (!w.all_) {
			n &= w.options_;
		}
		if (n) {
			w.notifier_(w.handler_, std::move(n));
		}
	}
}

void COptionsBase::watch(optionsIndex opt, std::tuple<void*, watcher_notifier> handler)
{
	if (!std::get<0>(handler) || !std::get<1>(handler)|| opt == optionsIndex::invalid) {
		return;
	}

	for (size_t i = 0; i < watchers_.size(); ++i) {
		if (watchers_[i].handler_ == std::get<0>(handler)) {
			watchers_[i].options_.set(opt);
			return;
		}
	}
	watcher w;
	w.handler_ = std::get<0>(handler);
	w.notifier_ = std::get<1>(handler);
	w.options_.set(opt);
	watchers_.push_back(w);
}

void COptionsBase::watch_all(std::tuple<void*, watcher_notifier> handler)
{
	if (!std::get<0>(handler)) {
		return;
	}

	for (size_t i = 0; i < watchers_.size(); ++i) {
		if (watchers_[i].handler_ == std::get<0>(handler)) {
			watchers_[i].all_ = true;
			return;
		}
	}
	watcher w;
	w.handler_ = std::get<0>(handler);
	w.notifier_ = std::get<1>(handler);
	w.all_ = true;
	watchers_.push_back(w);
}

void COptionsBase::unwatch(optionsIndex opt, std::tuple<void*, watcher_notifier> handler)
{
	if (!std::get<0>(handler) || opt == optionsIndex::invalid) {
		return;
	}

	for (size_t i = 0; i < watchers_.size(); ++i) {
		if (watchers_[i].handler_ == std::get<0>(handler)) {
			watchers_[i].options_.unset(opt);
			if (watchers_[i].options_ || watchers_[i].all_) {
				return;
			}
			watchers_[i] = watchers_.back();
			watchers_.pop_back();
			return;
		}
	}
}

void COptionsBase::unwatch_all(std::tuple<void*, watcher_notifier> handler)
{
	if (!std::get<0>(handler) || !std::get<1>(handler)) {
		return;
	}

	for (size_t i = 0; i < watchers_.size(); ++i) {
		if (watchers_[i].handler_ == std::get<0>(handler)) {
			watchers_[i] = watchers_.back();
			watchers_.pop_back();
			return;
		}
	}
}
