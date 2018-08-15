#ifndef _MY_STANDARD_AFX_INCLUDER
#define _MY_STANDARD_AFX_INCLUDER

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WINVER 0x0400		// Change this to 0x0500 to target Windows 98 and Windows 2000.
#define _WIN32_WINNT 0x0400	// Change this to 0x0500 to target Windows 2000.
#define _WIN32_WINDOWS 0x0410
#define _WIN32_IE 0x0400	// Change this to 0x0500 to target IE 5.0.
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#include <winerror.h>
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#endif //!_MY_STANDARD_AFX_INCLUDER