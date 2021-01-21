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

int64_t ConvertToVersionNumber(wchar_t const* version)
{
	// Crude conversion from version string into number for easy comparison
	// Supported version formats:
	// 1.2.4
	// 11.22.33.44
	// 1.2.3-rc3
	// 1.2.3.4-beta5
	// All numbers can be as large as 1024, with the exception of the release candidate.

	// Only either rc or beta can exist at the same time)
	//
	// The version string A.B.C.D-rcE-betaF expands to the following binary representation:
	// 0000aaaaaaaaaabbbbbbbbbbccccccccccddddddddddxeeeeeeeeeffffffffff
	//
	// x will be set to 1 if neither rc nor beta are set. 0 otherwise.
	//
	// Example:
	// 2.2.26-beta3 will be converted into
	// 0000000010 0000000010 0000011010 0000000000 0000000000 0000000011
	// which in turn corresponds to the simple 64-bit number 2254026754228227
	// And these can be compared easily

	if (!version || *version < '0' || *version > '9') {
		return -1;
	}

	int64_t v{};
	int segment{};
	int shifts{};

	for (; *version; ++version) {
		if (*version == '.' || *version == '-' || *version == 'b') {
			v += segment;
			segment = 0;
			v <<= 10;
			shifts++;
		}
		if (*version == '-' && shifts < 4) {
			v <<= (4 - shifts) * 10;
			shifts = 4;
		}
		else if (*version >= '0' && *version <= '9') {
			segment *= 10;
			segment += *version - '0';
		}
	}
	v += segment;
	v <<= (5 - shifts) * 10;

	// Make sure final releases have a higher version number than rc or beta releases
	if ((v & 0x0FFFFF) == 0) {
		v |= 0x080000;
	}

	return v;
}
