#include "fz_paths.h"

#include "xml_file.h"
#include <libfilezilla/local_filesys.hpp>

#include <cstdlib>
#include <cstring>

#ifdef FZ_MAC
	#include <mach-o/dyld.h>
#elif defined(FZ_WINDOWS)
	#include <shlobj.h>

	// Needed for MinGW:
	#ifndef SHGFP_TYPE_CURRENT
		#define SHGFP_TYPE_CURRENT 0
	#endif
#else
	#include <unistd.h>
#endif

static std::wstring GetEnv(char const* name)
{
	std::wstring ret;
	if (name) {
		auto* v = getenv(name);
		if (v) {
			ret = fz::to_wstring(v);
		}
	}
	return ret;
}

#ifndef FZ_WINDOWS
namespace {
static std::wstring TryDirectory(std::wstring path, std::wstring const& suffix, bool check_exists)
{
	if (!path.empty() && path[0] == '/') {
		if (path[path.size() - 1] != '/') {
			path += '/';
		}

		path += suffix;

		if (check_exists) {
			if (!CLocalPath(path).Exists(nullptr)) {
				path.clear();
			}
		}
	}
	else {
		path.clear();
	}
	return path;
}
}
#endif

CLocalPath GetHomeDir()
{
	CLocalPath ret;

#ifdef FZ_WINDOWS
	wchar_t buffer[MAX_PATH * 2 + 1];
	if (SUCCEEDED(SHGetFolderPath(0, CSIDL_PROFILE, 0, SHGFP_TYPE_CURRENT, buffer))) {
		ret.SetPath(buffer);
	}
#else
	ret.SetPath(GetEnv("HOME"));
#endif
	return ret;
}

CLocalPath GetUnadjustedSettingsDir()
{
	CLocalPath ret;

#ifdef FZ_WINDOWS
	wchar_t buffer[MAX_PATH * 2 + 1];

	if (SUCCEEDED(SHGetFolderPath(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, buffer))) {
		CLocalPath tmp(buffer);
		if (!tmp.empty()) {
			tmp.AddSegment(L"FileZilla");
		}
		ret = tmp;
	}

	if (ret.empty()) {
		// Fall back to directory where the executable is
		DWORD c = GetModuleFileName(0, buffer, MAX_PATH * 2);
		if (c && c < MAX_PATH * 2) {
			std::wstring tmp;
			ret.SetPath(buffer, &tmp);
		}
	}
#else
	std::wstring cfg = TryDirectory(GetEnv("XDG_CONFIG_HOME"), L"filezilla/", true);
	if (cfg.empty()) {
		cfg = TryDirectory(GetEnv("HOME"), L".config/filezilla/", true);
	}
	if (cfg.empty()) {
		cfg = TryDirectory(GetEnv("HOME"), L".filezilla/", true);
	}
	if (cfg.empty()) {
		cfg = TryDirectory(GetEnv("XDG_CONFIG_HOME"), L"filezilla/", false);
	}
	if (cfg.empty()) {
		cfg = TryDirectory(GetEnv("HOME"), L".config/filezilla/", false);
	}
	if (cfg.empty()) {
		cfg = TryDirectory(GetEnv("HOME"), L".filezilla/", false);
	}
	ret.SetPath(cfg);
#endif
	return ret;
}

std::wstring GetOwnExecutableDir()
{
#ifdef FZ_WINDOWS
	// Add executable path
	std::wstring path;
	path.resize(4095);
	DWORD res;
	while (true) {
		res = GetModuleFileNameW(0, &path[0], path.size() - 1);
		if (!res) {
			// Failure
			return std::wstring();
		}

		if (res < path.size() - 1) {
			path.resize(res);
			break;
		}

		path.resize(path.size() * 2 + 1);
	}
	size_t pos = path.find_last_of(L"\\/");
	if (pos != std::wstring::npos) {
		return path.substr(0, pos + 1);
	}
#elif defined(FZ_MAC)
	#warning untested implementation
	//TODO: test this code!
	std::uint32_t size = 0;
	if (_NSGetExecutablePath(nullptr, &size) == -1) {
		std::vector<char> buf;
		buf.resize(size);
		if (_NSGetExecutablePath(buf.data(), &size) == 0) {
			char canonical[PATH_MAX];
			if (realpath(buf.data(), canonical)) {
				std::wstring executable = fz::to_wstring(std::string(canonical));
				size_t pos = executable.rfind('/');
				if (pos != std::wstring::npos) {
					return executable.substr(0, pos + 1);
				}
			}
		}
	}
#else
	std::string path;
	path.resize(4095);
	while (true) {
		int res = readlink("/proc/self/exe", &path[0], path.size());
		if (res < 0) {
			return std::wstring();
		}
		if (static_cast<size_t>(res) < path.size()) {
			path.resize(res);
			break;
		}
		path.resize(path.size() * 2 + 1);
	}
	size_t pos = path.rfind('/');
	if (pos != std::wstring::npos) {
		return fz::to_wstring(path.substr(0, pos + 1));
	}
#endif
	return std::wstring();
}

#ifdef FZ_MAC
std::wstring mac_data_path();
#endif

namespace {
static bool FileExists(std::wstring const& file) {
	return fz::local_filesys::get_file_type(fz::to_native(file), true) == fz::local_filesys::file;
}

#if FZ_WINDOWS
std::wstring const PATH_SEP = L";";
#define L_DIR_SEP L"\\"
#else
std::wstring const PATH_SEP = L":";
#define L_DIR_SEP L"/"
#endif
}

CLocalPath GetFZDataDir(std::vector<std::wstring> const& fileToFind, std::wstring const& prefixSub, bool searchSelfDir)
{
	/*
	 * Finding the resources in all cases is a difficult task,
	 * due to the huge variety of different systems and their filesystem
	 * structure.
	 * Basically we just check a couple of paths for presence of the resources,
	 * and hope we find them. If not, the user can still specify on the cmdline
	 * and using environment variables where the resources are.
	 *
	 * At least on OS X it's simple: All inside application bundle.
	 */

	CLocalPath ret;

	auto testPath = [&](std::wstring const& path) {
		ret = CLocalPath(path);
		if (ret.empty()) {
			return false;
		}

		for (auto const& file : fileToFind) {
			if (FileExists(ret.GetPath() + file)) {
				return true;
			}
		}
		return false;
	};

#ifdef FZ_MAC
	(void)prefixSub;

	if (searchSelfDir && testPath(mac_data_path())) {
		return ret;
	}

	return CLocalPath();
#else

	// First try the user specified data dir.
	if (searchSelfDir) {
		if (testPath(GetEnv("FZ_DATADIR"))) {
			return ret;
		}
	}

	std::wstring selfDir = GetOwnExecutableDir();
	if (!selfDir.empty()) {
		if (searchSelfDir && testPath(selfDir)) {
			return ret;
		}

		if (!prefixSub.empty() && selfDir.size() > 5 && fz::ends_with(selfDir, std::wstring(L_DIR_SEP L"bin" L_DIR_SEP))) {
			std::wstring path = selfDir.substr(0, selfDir.size() - 4) + prefixSub + L_DIR_SEP;
			if (testPath(path)) {
				return ret;
			}
		}

		// Development paths
		if (searchSelfDir && selfDir.size() > 7 && fz::ends_with(selfDir, std::wstring(L_DIR_SEP L".libs" L_DIR_SEP))) {
			std::wstring path = selfDir.substr(0, selfDir.size() - 6);
			if (FileExists(path + L"Makefile")) {
				if (testPath(path)) {
					return ret;
				}
			}
		}
	}

	// Now scan through the path
	if (!prefixSub.empty()) {
		std::wstring path = GetEnv("PATH");
		auto const segments = fz::strtok(path, PATH_SEP);

		for (auto const& segment : segments) {
			auto const cur = CLocalPath(segment).GetPath();
			if (cur.size() > 5 && fz::ends_with(cur, std::wstring(L_DIR_SEP L"bin" L_DIR_SEP))) {
				std::wstring path = cur.substr(0, cur.size() - 4) + prefixSub + L_DIR_SEP;
				if (testPath(path)) {
					return ret;
				}
			}
		}
	}

	ret.clear();
	return ret;
#endif
}

CLocalPath GetDefaultsDir()
{
	CLocalPath path;
#ifdef FZ_UNIX
	path = GetUnadjustedSettingsDir();
	if (path.empty() || !FileExists(path.GetPath() + L"fzdefaults.xml")) {
		if (FileExists(L"/etc/filezilla/fzdefaults.xml")) {
			path.SetPath(L"/etc/filezilla");
		}
		else {
			path.clear();
		}
	}

#endif
	if (path.empty()) {
		path = GetFZDataDir({L"fzdefaults.xml"}, L"share/filezilla");
	}

	return path;
}


std::wstring GetSettingFromFile(std::wstring const& xmlfile, std::string const& name)
{
	CXmlFile file(xmlfile);
	if (!file.Load()) {
		return L"";
	}

	auto element = file.GetElement();
	if (!element) {
		return L"";
	}

	auto settings = element.child("Settings");
	if (!settings) {
		return L"";
	}

	for (auto setting = settings.child("Setting"); setting; setting = setting.next_sibling("Setting")) {
		const char* nodeVal = setting.attribute("name").value();
		if (!nodeVal || strcmp(nodeVal, name.c_str())) {
			continue;
		}

		return fz::to_wstring_from_utf8(setting.child_value());
	}

	return L"";
}

std::wstring ReadSettingsFromDefaults(CLocalPath const& defaultsDir)
{
	if (defaultsDir.empty()) {
		return L"";
	}

	std::wstring dir = GetSettingFromFile(defaultsDir.GetPath() + L"fzdefaults.xml", "Config Location");
	auto result = ExpandPath(dir);

	if (!FileExists(result)) {
		return L"";
	}

	if (result[result.size() - 1] != '/') {
		result += '/';
	}

	return result;
}


CLocalPath GetSettingsDir()
{
	CLocalPath p;

	CLocalPath const defaults_dir = GetDefaultsDir();
	std::wstring dir = ReadSettingsFromDefaults(defaults_dir);
	if (!dir.empty()) {
		dir = ExpandPath(dir);
		p.SetPath(defaults_dir.GetPath());
		p.ChangePath(dir);
	}
	else {
		p = GetUnadjustedSettingsDir();
	}
	return p;
}

namespace {
template<typename String>
String ExpandPathImpl(String dir)
{
	if (dir.empty()) {
		return dir;
	}

	String result;
	while (!dir.empty()) {
		String token;
#ifdef FZ_WINDOWS
		size_t pos = dir.find_first_of(fzS(typename String::value_type, "\\/"));
#else
		size_t pos = dir.find('/');
#endif
		if (pos == String::npos) {
			token.swap(dir);
		}
		else {
			token = dir.substr(0, pos);
			dir = dir.substr(pos + 1);
		}

		if (token[0] == '$') {
			if (token[1] == '$') {
				result += token.substr(1);
			}
			else if (token.size() > 1) {
				char* env = getenv(fz::to_string(token.substr(1)).c_str());
				if (env) {
					result += fz::toString<String>(env);
				}
			}
		}
		else {
			result += token;
		}

#ifdef FZ_WINDOWS
		result += '\\';
#else
		result += '/';
#endif
	}

	return result;
}
}

std::string ExpandPath(std::string const& dir) {
	return ExpandPathImpl(dir);
}

std::wstring ExpandPath(std::wstring const& dir) {
	return ExpandPathImpl(dir);
}
