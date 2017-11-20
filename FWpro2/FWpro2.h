#pragma once

#include "resource.h"

// states
#define FW_LAUNCH			0
#define FW_IDLE				1
#define FW_RUNNING			2
#define FW_PAUSED			3
#define FW_BUSY				4

// options
typedef struct
{
	BOOL loopOnError;
	DWORD loopDelay;
}OptionsStruct;

// registry defines for options
typedef struct
{
	LPCSTR valueName;
	DWORD getType;
	DWORD setType;
	PVOID value;
	DWORD size;
}RegDefOpt;