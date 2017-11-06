#include "stdafx.h"
using namespace std;

void log(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vprintf((string(format) + "\n").c_str(), args);

	va_end(args);
}

void logE(const char *format, ...)
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();

	LPSTR msg = nullptr;
	size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);

	// concatenate the strings
	va_list args;
	va_start(args, format);

	vprintf((string(format) + " - " + string(msg)).c_str(), args);

	va_end(args);

	//Free the buffers.
	LocalFree(msg);
}