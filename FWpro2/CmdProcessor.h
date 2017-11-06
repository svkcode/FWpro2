#pragma once
#include "SymbolTable.h"
using namespace std;

// Parameter types
#define PT_NUMBER		0
#define PT_STRING		1
#define PT_BYTEARRAY	2
#define PT_FILEOPEN		3
#define PT_FILESAVE		4

struct paramInfo
{
	string name;
	int type;
};

enum State
{
	run, stop
};

typedef BOOL(*CmdFxn)(vector<string>, SymbolTable&, State&);

class CmdProcessor
{
	HANDLE hScript;
	vector<paramInfo> paramOrder;
	SymbolTable sTable;
	DWORD scriptCmdOffset;
	DWORD scriptLoopOffset;
	DWORD prevFilePointer;
	BOOL _loop;
	State state;
	vector<string> scriptNxtLine(void);
	BOOL addParam(vector<string>);
	void processCmds(void);
	static unordered_map<string, CmdFxn> fcnDispatch;
public:
	CmdProcessor(void);
	BOOL open(LPSTR);
	void run(void);
	void paramsInit(HWND);
	void paramsSave(HWND);
	void paramsSaveAs(HWND);
	void paramsOK(HWND);
	void setLoop(BOOL);
	void stop(void);
};
