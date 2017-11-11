#pragma once
using namespace std;


// Data types
#define DT_NUMBER		1
#define DT_STRING		2
#define DT_BYTEARRAY	4
#define DT_BINARY		8


// Symbol data
struct symbol
{
	int dataType;
	void *ptr;
};

typedef unordered_map<string, symbol> symbolTable;

class SymbolTable
{
	 symbolTable sTable;	
public:
	void* newSymbol(string name, int dataType, int length);
	void* newSymbol(string name, int dataType);
	void* addSymbol(string name, int dataType, int length);
	void* addSymbol(string name, int dataType);
	void* getSymbol(string name, int dataType);
	void* getSymbol(string name, int *dataType);
	void deleteSymbol(string name);
	void clear(void);
	int getLength(void *sym, int dataType);
	string toString(void *sym, int dataType);
};