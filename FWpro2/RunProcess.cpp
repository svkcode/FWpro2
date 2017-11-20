#include "stdafx.h"
#include "CmdProcessor.h"
#include "Log.h"
#include "RunProcess.h"
using namespace std;


BOOL runProcess(char *appName, char *cmdLine, State &state, string *output, BOOL showWindow)
{
	// Handles to stdout pipe
	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;

	// Create a pipe for the child process's STDOUT. 
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
	{
		logE("Failed to create pipe to stdout");
		return FALSE;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		logE("Failed to set pipe handle information");
		CloseHandle(g_hChildStd_OUT_Rd);
		CloseHandle(g_hChildStd_OUT_Wr);
		return FALSE;
	}

	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;

	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	// redirect stdout only if window is hidden
	if (!showWindow)
		siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Get current directory of the application
	TCHAR temp[MAX_PATH];
	GetFullPathName(appName, MAX_PATH, temp, NULL);
	string file(temp);
	const char *directory = NULL;
	int n = file.find_last_of("/\\");
	if (n != string::npos)
	{
		file = file.substr(0, n);
		directory = file.c_str();
	}

	// Launch the process in debug mode so that it is 
	// automatically terminated if the app is closed
	DWORD cFlags = DEBUG_PROCESS; // NULL
	if (!showWindow) cFlags |= CREATE_NO_WINDOW;
	
	// Create the child process. 
	if (!CreateProcess(appName, cmdLine, NULL, NULL, TRUE, cFlags, NULL, directory, &siStartInfo, &piProcInfo))
	{
		logE("Unable to run %s", PathFindFileName(appName));
		CloseHandle(g_hChildStd_OUT_Rd);
		CloseHandle(g_hChildStd_OUT_Wr);
		return FALSE;
	}


	DWORD dwRead, bytesAvailable, wFSO;
	CHAR chBuf[256];
	BOOL stopping = FALSE;
	DEBUG_EVENT DebugEv;

	while (true)
	{
		// Check if user stopped the execution
		if (state == State::stop && !stopping)
		{
			TerminateProcess(piProcInfo.hProcess, -1);
			stopping = TRUE;
		}

		// Check for process exit (1ms timeout to reduce CPU load)
		wFSO = WaitForSingleObject(piProcInfo.hProcess, 1);

		// Resume process on debug events		
		if(WaitForDebugEvent(&DebugEv, 0))
			ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, DBG_CONTINUE);
		
		// If bytes available, move them to calling process' stdout
		PeekNamedPipe(g_hChildStd_OUT_Rd, NULL, NULL, NULL, &bytesAvailable, NULL);
		if (bytesAvailable > 0)
		{
			if (ReadFile(g_hChildStd_OUT_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL))
			{
				// Terminate with null char
				chBuf[dwRead] = '\0';
				printf("%s", chBuf);
				// Append to output string if not NULL
				if (output != NULL) *output += chBuf;
			}
			else logE("Stdout pipe read error");
			
		}
		if (wFSO == WAIT_OBJECT_0) break;
		else if (wFSO == WAIT_FAILED)
		{
			logE("Wait for process failed");
			//break;
		}
	}

	DWORD exitCode; 
	if (!GetExitCodeProcess(piProcInfo.hProcess, &exitCode))
		exitCode = 100;

	// Close process and pipe handles. 
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
	CloseHandle(g_hChildStd_OUT_Rd);
	CloseHandle(g_hChildStd_OUT_Wr);

	if (exitCode == 0)
	{
		log("%s run completed", PathFindFileName(appName));
		return TRUE;
	}
	else if (exitCode == -1)
	{
		log("%s stopped", PathFindFileName(appName));
		return FALSE;
	}
	log("%s returned an error", PathFindFileName(appName));
	return FALSE;
}
