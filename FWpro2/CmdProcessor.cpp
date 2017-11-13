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

BOOL CmdProcessor::paramsInit(HWND hDlg)
{
	const UINT xo = 5;			// x-offset from edge
	const UINT textW = 120;		// width of static text
	const UINT editW = 250;		// width of edit control
	const UINT TEspace = 10;	// spacing between text & edit
	const UINT ySpace = 25;		// spacing between lines
	UINT y = 5;					// current line
	UINT id = 1;				// resource ID for controls

	// Create font for text & edit
	HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, OEM_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	HWND hText, hEdit;
	UINT buttonID;

	// Check if there are params
	if (paramOrder.empty())
	{
		MessageBox(hDlg, "There are no parameters in the script to display", "FWpro2", MB_OK);
		return FALSE;
	}

	// Iterate & add all parameters to the dialog
	for (auto it : paramOrder)
	{
		hText = CreateWindowEx(NULL, "Static", it.name.c_str(), WS_VISIBLE | WS_CHILD | SS_RIGHT, xo, y, textW, 20, hDlg, NULL, NULL, NULL);
		hEdit = CreateWindowEx(0, "EDIT", NULL, WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			xo + textW + TEspace, y, editW, 19, hDlg, (HMENU)(IDD_PROPDLG | id), NULL, NULL);
		
		// Add button for file open & save dialogs
		if (it.type & (PT_FILEOPEN | PT_FILESAVE))
		{
			if (it.type == PT_FILEOPEN) buttonID = IDB_PROPOPEN; else buttonID = IDB_PROPSAVE;

			CreateWindowEx(NULL, "BUTTON", "...", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
				xo + textW + TEspace + editW - 1, y, 22, 19, hDlg, (HMENU)(buttonID | id), NULL, NULL);
		}

		// Set text & edit font
		SendMessage(hText, WM_SETFONT, WPARAM(hFont), TRUE);
		SendMessage(hEdit, WM_SETFONT, WPARAM(hFont), TRUE);
		 
		// Display the stored symbol value
		int dataType;
		void *sym = sTable.getSymbol(it.name, &dataType);
		if(sym == NULL)
			SetWindowText(hEdit, "--Symbol not found--");
		else
		{
			string value = sTable.toString(sym, dataType);
			if (it.type == PT_HEXNUMBER)
			{
				char buf[8]; // int has max 8 nibbles
				sprintf_s(buf, sizeof(buf), "%04x", *((UINT *)sym));
				value = "0x" + string(buf);
			}
			SetWindowText(hEdit, value.c_str());
		}

		// Increment current y
		y += ySpace;

		// Move the cursor to end for strings that exceed edit width
		SendMessage(hEdit, EM_SETSEL, 0, -1);
		SendMessage(hEdit, EM_SETSEL, -1, -1);

		// Increment resource ID
		id++;
	}

	// Update dialog size & re-position buttons
	UINT totalW = xo + textW + TEspace + editW + 100;
	SetWindowPos(hDlg, HWND_TOP, 0, 0, totalW, y + 100, SWP_NOMOVE);
	SetWindowPos(GetDlgItem(hDlg, IDOK), HWND_TOP, 70, y + 30, 0, 0, SWP_NOSIZE);
	SetWindowPos(GetDlgItem(hDlg, IDB_SAVE), HWND_TOP, totalW / 2 - 45, y + 30, 0, 0, SWP_NOSIZE);
	SetWindowPos(GetDlgItem(hDlg, IDB_SAVEAS), HWND_TOP, totalW - 70 - 90, y + 30, 0, 0, SWP_NOSIZE);

	return TRUE;
}

string CmdProcessor::PT2String(UINT paramType)
{
	switch (paramType)
	{
	case PT_NUMBER:
	case PT_HEXNUMBER:
		return string("number");
	case PT_STRING:
		return string("string");
	case PT_BYTEARRAY:
		return string("byteArray");
	case PT_FILEOPEN:
		return string("fileOpen");
	case PT_FILESAVE:
		return string("fileSave");
	default:
		return NULL;
	}
}

BOOL CmdProcessor::paramsFromDlg(HWND hDlg, vector<paramInfo> poSrc, SymbolTable &sTable,  vector<paramInfo> &poDst, string *params)
{
	UINT id = 1;
	vector<string> cmdlets(4);
	cmdlets[0] = "param";
	for (auto it : poSrc)
	{
		HWND hEdit = GetDlgItem(hDlg, IDD_PROPDLG | id);
		int len = GetWindowTextLength(hEdit);
		char *buf = new char[len + 1];
		GetWindowText(hEdit, buf, len + 1);
		cmdlets[1] = PT2String(it.type);
		cmdlets[2] = it.name;
		cmdlets[3] = string(buf);
		delete[] buf;
		if (!addParam(cmdlets, sTable, poDst))
		{
			MessageBox(hDlg, "Invalid parameter value. Check log for details", "FWpro2", MB_OK);
			return FALSE;
		}
		// Compile a script statement for save option
		if (params != NULL)
		{
			for (int i = 2; i < 4; i++)
			{
				// enclose single quotes if there are double quotes
				if (cmdlets[i].find("\"") != string::npos) cmdlets[i] = "\'" + cmdlets[i] + "\'";
				// enclose double quotes if there are spaces
				else if (cmdlets[i].find(" ") != string::npos) cmdlets[i] = "\"" + cmdlets[i] + "\"";
			}
			params->append(cmdlets[0] + " " + cmdlets[1] + " " + cmdlets[2] + " " + cmdlets[3] + "\r\n");
		}
		id++;
	}
	return TRUE;
}

BOOL CmdProcessor::paramsOK(HWND hDlg)
{
	return paramsOK(hDlg, NULL);
}

BOOL CmdProcessor::paramsOK(HWND hDlg, string *params)
{	
	SymbolTable st;			// Temp symbol table & param order
	vector<paramInfo> po;
	// First pass for error checking so that original 
	// symbol table & param order are not modified
	if (!paramsFromDlg(hDlg, paramOrder, st, po, NULL))
	{
		st.clear();
		return FALSE;
	}
	st.clear();
	sTable.clear();
	paramOrder.clear();
	// shouldn't return false
	if (!paramsFromDlg(hDlg, po, sTable, paramOrder, params)) return FALSE;
	log("Parameters updated");
	return TRUE;
}

BOOL CmdProcessor::paramsSave(HWND hDlg)
{
	string line;
	if (!paramsOK(hDlg, &line)) return FALSE;
	log(line.c_str());
	return TRUE;
}

BOOL CmdProcessor::paramsSaveAs(HWND hDlg, LPSTR file)
{
	return TRUE;
}


BOOL CmdProcessor::open(LPSTR fileName)
{
	if (hScript != NULL)
	{
		CloseHandle(hScript);
		hScript = NULL;
	}
	hScript = CreateFile(fileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
		if (!addParam(cmdlets, sTable, paramOrder)) return FALSE;
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



BOOL CmdProcessor::addParam(vector<string> cmdlets, SymbolTable &sTable, vector<paramInfo> &paramOrder)
{
	int type;

	if (cmdlets[1] == "number")
	{
		if (cmdlets[3].substr(0, 2) == "0x" || cmdlets[3].substr(0, 2) == "0X")
		{
			if (cmdlets[3].substr(2, string::npos).find_first_not_of("0123456789aAbBcCdDeEfF") != string::npos)
			{
				log("Invalid character in hex number parameter : %s", cmdlets[2].c_str());
				return FALSE;
			}
			type = PT_HEXNUMBER;
		}
		else
		{
			if (cmdlets[3].find_first_not_of("0123456789+-") != string::npos)
			{
				log("Invalid character in number parameter : %s", cmdlets[2].c_str());
				return FALSE;
			}
			type = PT_NUMBER;
		}
		
		int *ptr = (int *)sTable.newSymbol(cmdlets[2], DT_NUMBER);
		*ptr = strtol(cmdlets[3].c_str(), NULL, 0);
	}
	else if (cmdlets[1] == "string")
	{
		type = PT_STRING;
	}
	else if (cmdlets[1] == "fileOpen")
	{
		type = PT_FILEOPEN;
	}
	else if (cmdlets[1] == "fileSave")
	{
		type = PT_FILESAVE;
	}
	else if (cmdlets[1] == "byteArray")
	{
		UINT len = cmdlets[3].size();
		if (len % 2 != 0)
		{
			log("Invalid byteArray parameter length : %s", cmdlets[2].c_str());
			return FALSE;
		}
		if (cmdlets[3].find_first_not_of("0123456789aAbBcCdDeEfF") != string::npos)
		{
			log("Invalid character in byteArray parameter : %s", cmdlets[2].c_str());
			return FALSE;
		}
		type = PT_BYTEARRAY;
		char *ba = (char *)sTable.newSymbol(cmdlets[2], DT_BYTEARRAY, len / 2);
		for (UINT i = 0; i < len; i += 2)
			*ba++ = (char)strtol(cmdlets[3].substr(i, 2).c_str(), NULL, 16);
	}
	else
	{ 
		log("Invalid parameter: %s",cmdlets[1].c_str());
		return FALSE;
	}

	if (type & (PT_STRING | PT_FILEOPEN | PT_FILESAVE))
	{
		char *str = (char *)sTable.newSymbol(cmdlets[2], DT_STRING, cmdlets[3].length() + 1);
		memcpy(str, cmdlets[3].c_str(), cmdlets[3].length() + 1);
	}

	// store order of insertion in vector
	paramOrder.push_back({ cmdlets[2], type });
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