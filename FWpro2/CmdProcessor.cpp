#include "stdafx.h"
#include "Resource.h"
#include "CmdFxnDefs.h"
#include "SymbolTable.h"
#include "CmdProcessor.h"
#include "Log.h"
using namespace std;

CmdProcessor::CmdProcessor(void)
{
	// init state variables
	_loop = FALSE;
	state = State::run;
}

void CmdProcessor::paramsInit(HWND hDlg)
{
	//CreateWindowEx(0, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
	//	10, 10, 50, 50, hDlg, (HMENU)200, NULL, NULL);
}



BOOL CmdProcessor::open(LPSTR fileName)
{
	if (hScript != NULL)
	{
		CloseHandle(hScript);
		hScript = NULL;
	}
	hScript = CreateFileA(fileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hScript == INVALID_HANDLE_VALUE)
	{
		logE("Unable to open file %s",fileName);
		return FALSE;
	}
	// clear the symbol table
	sTable.clear();
	paramOrder.clear();
	// read the params into the symbol table
	vector<string> cmdlets;	
	while(true)
	{
		cmdlets = scriptNxtLine();
		if (cmdlets == (vector<string>)NULL) return FALSE;
		// end of params
		if (cmdlets[0] != "param") 
		{
			// store the offset to beginning of cmds
			scriptCmdOffset = prevFilePointer;
			// loop offset is the same for now as a backup
			scriptLoopOffset = prevFilePointer;
			break;
		}
		// add to symbol table
		if (!addParam(cmdlets)) return FALSE;
	}
	log("Parameters loaded");
	return TRUE;
}


vector<string> CmdProcessor::scriptNxtLine(void)
{
	CHAR buffer[1024];
	DWORD nBytesRead;
	vector<string> cmdlets;
read:
	// store the current file pointer before reading further
	prevFilePointer = SetFilePointer(hScript, 0, NULL, FILE_CURRENT);
	if (!ReadFile(hScript, buffer, 1024, &nBytesRead, NULL))
	{
		logE("Unable to read script file");
		return (vector<string>)NULL;
	}
	if(nBytesRead == 0)
	{ 
		// cmd 'end' to indicate end of file
		cmdlets.push_back("end");
		return cmdlets;
	}
	size_t n, m = 0;
	string line(buffer, nBytesRead);
	// find end of line
	n = line.find("\r\n");
	// get substring of one line
	if (n != string::npos) 
	{
		line = line.substr(0, n);
		// set file pointer to beginning of next line
		if (SetFilePointer(hScript, n - nBytesRead + 2, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		{
			logE("Unable to read script file");
			return (vector<string>)NULL;
		}
	}
	// if no end of line found, then could be last line
	else line = line.substr(0, nBytesRead);
	// if this is an empty line, read the next line
	if ((n = line.find_first_not_of(" ")) == string::npos)
		goto read;
	// skip if this is a comment line
	if (line.at(n) == '/')
		goto read;
	// split line by spaces & quotes
	while (m < line.length())
	{
		// find first non-space char
		n = line.find_first_not_of(" ", m);
		// if not found, then must be EOL
		if (n == string::npos) break;
		// check if this char is a
		if (line.at(n) == '"')				// double quote
		{
			// if so, ignore all spaces until the closing quote
			n++;
			m = line.find("\"", n);
			if (m == string::npos) { log("Script syntax error: quotes mismatch"); return (vector<string>)NULL;}
		}
		else if (line.at(n) == '\'')		// single quote
		{
			n++;
			m = line.find("\'", n);
			if (m == string::npos) { log("Script syntax error: quotes mismatch"); return (vector<string>)NULL; }
		}
		else
		{
			// find the next space
			m = line.find(" ", n);
			// if not found, then must be the last cmdlet
			if (m == string::npos) m = line.length();
		}
		// add to vector
		cmdlets.push_back(line.substr(n, m-n));
		m++;
	}

	return cmdlets;
}



BOOL CmdProcessor::addParam(vector<string> cmdlets)
{
	int type;

	if (cmdlets[1] == "number")
	{
		type = PT_NUMBER;
		int *ptr = (int *)sTable.newSymbol(cmdlets[2], DT_NUMBER);
		*ptr = strtol(cmdlets[3].c_str(), NULL, 0);
	}
	else if (cmdlets[1] == "string")
	{
		type = PT_STRING;
		goto stringType;
	}
	else if (cmdlets[1] == "fileOpen")
	{
		type = PT_FILEOPEN;
		goto stringType;
	}
	else if (cmdlets[1] == "fileSave")
	{
		type = PT_FILESAVE;
		goto stringType;
	}
	else
	{ 
		log("Invalid parameter: %s",cmdlets[1].c_str());
		return FALSE;
	}

	// store order of insertion in vector
	paramOrder.push_back({ cmdlets[2], type });
	return TRUE;

stringType:
	char *str = (char *)sTable.newSymbol(cmdlets[2], DT_STRING, cmdlets[3].length() + 1);
	memcpy(str, cmdlets[3].c_str(), cmdlets[3].length() + 1);
	return TRUE;
}



void CmdProcessor::setLoop(BOOL enable)
{
	_loop = enable;
}

void CmdProcessor::stop(void)
{
	state = State::stop;
}

void CmdProcessor::run(void)
{
	processCmds();
	state = State::run;
	log("Stopped");
}

void CmdProcessor::processCmds(void)
{
	vector<string> cmdlets;
	DWORD scriptOffset = scriptCmdOffset;
start:
	// set file pointer to beginning of cmds
	if (SetFilePointer(hScript, scriptOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		logE("Unable to find the first command");
		return;
	}	
	log("Running...");
	while (state == State::run)
	{
		// read next line
		cmdlets = scriptNxtLine();
		// error
		if (cmdlets == (vector<string>)NULL) return;
		// end of commands
		if (cmdlets[0] == "end")
		{
			log("Completed");
			if (_loop)
			{
				log("Looping...");
				scriptOffset = scriptLoopOffset;
				Sleep(1000);
				goto start;
			}
			else return;
		}
		// loop directive
		if (cmdlets[0] == "loop")
		{
			scriptLoopOffset = prevFilePointer;
			continue;
		}
		// process commands
		try
		{
			if (!(*fcnDispatch.at(cmdlets[0]))(cmdlets, sTable, state)) return;
		}
		catch(out_of_range oor)
		{
			log("Unknown command: %s",cmdlets[0].c_str());
			return;
		}
	}	
}

unordered_map<string, CmdFxn> CmdProcessor::fcnDispatch({
	{ "hex2bin",	&cf_hex2bin },
	{ "mergebin",	&cf_mergebin },
	{ "savebin",	&cf_savebin },
	{ "run",		&cf_run },
	{ "substr",		&cf_substring },
	{ "replace",	&cf_replace },
	{ "add2bin",	&cf_add2bin },
	{ "length",		&cf_length },
	{ "rand",		&cf_rand },
	{ "randtable",	&cf_randtable },
	{ "crc16bin",	&cf_crc16bin},
	{ "replacebin",	&cf_replacebin},
	{ "add2csv",	&cf_add2csv},
	{ "replacecsv",	&cf_replacecsv}
});