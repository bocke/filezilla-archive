#ifndef FILEZILLA_ENGINE_VERSION_HEADER
#define FILEZILLA_ENGINE_VERSION_HEADER

#include "visibility.h"

#include <string>

enum class lib_dependency
{
	gnutls,
	count
};

std::wstring FZC_PUBLIC_SYMBOL GetDependencyName(lib_dependency d);
std::wstring FZC_PUBLIC_SYMBOL GetDependencyVersion(lib_dependency d);

std::wstring FZC_PUBLIC_SYMBOL GetFileZillaVersion();

#endif
