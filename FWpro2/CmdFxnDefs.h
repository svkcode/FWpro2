#pragma once
#include "stdafx.h"
#include "CmdProcessor.h"
#include "SymbolTable.h"
using namespace std;

BOOL cf_hex2bin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_mergebin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_add2bin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_savebin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_openbin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_run(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_runs(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_runw(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_substring(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_replace(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_length(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_rand(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_randtable(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_crc16bin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_replacebin(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_add2csv(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_replacecsv(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_findcsv(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_add2str(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_num2byte(vector<string> params, SymbolTable &sTable, State &state);
BOOL cf_str2byte(vector<string> params, SymbolTable &sTable, State &state);