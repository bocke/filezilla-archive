#ifndef FILEZILLA_ENGINE_MISC_HEADER
#define FILEZILLA_ENGINE_MISC_HEADER

#include "visibility.h"

#include <libfilezilla/event_handler.hpp>

std::string FZC_PUBLIC_SYMBOL ListTlsCiphers(std::string const& priority);

template<typename Derived, typename Base>
std::unique_ptr<Derived>
unique_static_cast(std::unique_ptr<Base>&& p)
{
	auto d = static_cast<Derived *>(p.release());
	return std::unique_ptr<Derived>(d);
}

#if FZ_WINDOWS
DWORD FZC_PUBLIC_SYMBOL GetSystemErrorCode();
fz::native_string FZC_PUBLIC_SYMBOL GetSystemErrorDescription(DWORD err);
#else
int FZC_PUBLIC_SYMBOL GetSystemErrorCode();
fz::native_string FZC_PUBLIC_SYMBOL GetSystemErrorDescription(int err);
#endif

namespace fz {

// Poor-man's tolower. Consider to eventually use libicu or similar
std::wstring FZC_PUBLIC_SYMBOL str_tolower(std::wstring const& source);
}

#endif
