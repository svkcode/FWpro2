#pragma once
#include "SymbolTable.h"
using namespace std;

// Parameter types
#define PT_NUMBER		1
#define PT_HEXNUMBER	2
#define PT_STRING		4
#define PT_BYTEARRAY	8
#define PT_FILEOPEN		0x10
#define PT_FILESAVE		0x20

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
	BOOL addParam(vector<string>, SymbolTable &sTable, vector<paramInfo> &paramOrder);
	void processCmds(void);
	static unordered_map<string, CmdFxn> fcnDispatch;
	string PT2String(UINT paramType);
	BOOL paramsFromDlg(HWND, vector<paramInfo>, SymbolTable&, vector<paramInfo>&, string*);
	BOOL paramsOK(HWND, string*);
public:
	CmdProcessor(void);
	BOOL open(LPSTR);
	void run(void);
	BOOL paramsInit(HWND);
	BOOL paramsSave(HWND);
	BOOL paramsSaveAs(HWND, LPSTR);
	BOOL paramsOK(HWND);
	void setLoop(BOOL);
	void stop(void);
};
