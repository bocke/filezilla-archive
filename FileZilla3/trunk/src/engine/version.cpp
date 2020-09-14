#include "filezilla.h"

#include "../include/version.h"

#include <libfilezilla/tls_layer.hpp>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

std::wstring GetDependencyVersion(lib_dependency d)
{
	switch (d) {
	case lib_dependency::gnutls:
		return fz::to_wstring(fz::tls_layer::get_gnutls_version());
	default:
		return std::wstring();
	}
}

std::wstring GetDependencyName(lib_dependency d)
{
	switch (d) {
	case lib_dependency::gnutls:
		return L"GnuTLS";
	default:
		return std::wstring();
	}
}

std::wstring GetFileZillaVersion()
{
#ifdef PACKAGE_VERSION
	return fz::to_wstring(std::string(PACKAGE_VERSION));
#else
	return L"unknown";
#endif
}
