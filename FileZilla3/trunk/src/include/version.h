#ifndef FILEZILLA_ENGINE_VERSION_HEADER
#define FILEZILLA_ENGINE_VERSION_HEADER

#include <string>

enum class lib_dependency
{
	gnutls,
	count
};

std::wstring GetDependencyName(lib_dependency d);
std::wstring GetDependencyVersion(lib_dependency d);

std::wstring GetFileZillaVersion();

#endif
