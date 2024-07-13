#if defined(WIN32) || defined(WIN64)

// Microsoft™ Windows™ dependent code

// There is one, and only one case where this is needed and that's in a Visual
// Studio™ environment.  Even building a Windows™ executable with MXE doesn't
// require this.  So, even though this kind of thing is POSIX, it can't be
// included in a Visual Studio™ environment without adding a 3rd party shim.
// So we've made our own shim.
//
// The shim is minimal because the code in RMAC that uses it is minimal.  If it
// gets expanded in the future for some reason, the shim will have to expand
// too.  :-/  But that will never happen, right?  ;-)
//

#include "dirent_lose.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static DIR dummy;

DIR * opendir(const char * name)
{
	BOOL test = ((GetFileAttributesA(name) & FILE_ATTRIBUTE_DIRECTORY) != INVALID_FILE_ATTRIBUTES);

	return (test ? &dummy : NULL);
}

int closedir(DIR * dir)
{
	return 0;
}

#endif
