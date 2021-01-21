#include <sys/types.h>
#include <Foundation/NSBundle.h>
#include <Foundation/NSURL.h>
#include <Foundation/Foundation.h>

#include <libfilezilla/string.hpp>

template<typename Func>
static std::wstring mac_path(Func func)
{
	NSBundle *bundle = [NSBundle mainBundle];
	if (!bundle) {
		return std::wstring();
	}

	NSURL* url = (NSURL*)Func(bundle);
	if (!url) {
		return std::wstring();
	}

	NSError *error;
	if (![url checkResourceIsReachableAndReturnError:&error]) {
		return std::wstring();
	}

	return fz::to_wstring_from_utf8(std::string(url.path.UTF8String));
}

std::wstring mac_data_path() {
	return 	mac_path(&CFBundleCopySharedSupportURL);
}

std::wstring mac_exe_path() {
	return 	mac_path(&CFBundleCopyExecutableURL);
}
