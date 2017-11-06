// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Commctrl.h>
#include <Commdlg.h>
#include <iostream>
#include "Shlwapi.h"
#pragma comment (lib, "Shlwapi.lib")
#include <Wincrypt.h>
#pragma comment(lib, "advapi32.lib")

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

// TODO: reference additional headers your program requires here
