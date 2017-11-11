#include "stdafx.h"
#include "SymbolTable.h"
#include "Binary.h"

void* SymbolTable::newSymbol(string name, int dataType, int length)
{
	// delete the symbol if already exists
	deleteSymbol(name);
	// allocate memory in heap to store the symbol data
	symbol s;
	s.dataType = dataType;
	switch (dataType)
	{
	case DT_NUMBER:
		s.ptr = (void *)new int;
		break;
	case DT_STRING:
		s.ptr = (void *)new char[length];
		break;
	case DT_BYTEARRAY:
	{
		char *temp = new char[length + sizeof(int)];
		*((int *)temp) = length;
		temp += sizeof(int);
		s.ptr = (void *)temp;
		break;
	}
	case DT_BINARY:
		s.ptr = (void *)new Binary();
		break;
	default:
		return NULL;
	}
	// add the entry to the table
	sTable[name] = s;
	return s.ptr;
}

void* SymbolTable::newSymbol(string name, int dataType)
{
	return newSymbol(name, dataType, 0);
}

void* SymbolTable::addSymbol(string name, int dataType, int length)
{
	void *ptr;
	// return ptr if exists else create new
	if ((ptr = getSymbol(name, dataType)) != NULL)
		return ptr;
	else
		return newSymbol(name, dataType, length);
}

void* SymbolTable::addSymbol(string name, int dataType)
{
	return addSymbol(name, dataType, 0);
}

void* SymbolTable::getSymbol(string name, int dataType)
{
	symbolTable::const_iterator it = sTable.find(name);
	if (it == sTable.end()) return NULL;
	// check if data types match
	if (it->second.dataType == dataType)
		return it->second.ptr;
	else
		return NULL;
}

void* SymbolTable::getSymbol(string name, int *dataType)
{
	symbolTable::const_iterator it = sTable.find(name);
	if (it == sTable.end()) return NULL;
	// return data type
	*dataType = it->second.dataType;
	return it->second.ptr;
}

void SymbolTable::deleteSymbol(string name)
{
	symbolTable::const_iterator it = sTable.find(name);
	// return if doesn't exist
	if (it == sTable.end()) return;
	// first free the heap occupied by the symbol data
	switch (it->second.dataType)
	{
	case DT_NUMBER:
		delete (char *)it->second.ptr;
		break;
	case DT_STRING:
		delete[] (char *)it->second.ptr;
		break;
	case DT_BYTEARRAY:
	{
		char *temp = (char *)it->second.ptr;
		temp -= sizeof(int);
		delete[] temp;
		break;
	}
	case DT_BINARY:
		delete (Binary *)it->second.ptr;
		break;
	default:
		// shouldn't reach here
		return;
	}
	// remove the entry from the table
	sTable.erase(it);
}

void SymbolTable::clear(void)
{
	while(!sTable.empty())
		deleteSymbol(sTable.begin()->first);
	sTable.clear();
}

int SymbolTable::getLength(void *sym, int dataType)
{
	if (sym == NULL) return 0;
	int length;
	switch (dataType)
	{
	case DT_NUMBER:
		length = sizeof(int);
		break;
	case DT_STRING:
		length = strlen((char *)sym) + 1;
		break;
	case DT_BYTEARRAY:
	{
		char *temp = (char *)sym;
		temp -= sizeof(int);
		length = *((int *)temp);
		break;
	}
	case DT_BINARY:
		length = ((Binary *)sym)->length();
		break;
	default:
		// shouldn't reach here
		return 0;
	}
	return length;
}

string SymbolTable::toString(void *sym, int dataType)
{
	if (sym == NULL) return NULL;
	string str;
	switch (dataType)
	{
	case DT_NUMBER:
		str = to_string(*((int *)sym));
		break;
	case DT_STRING:
		str = string((char *)sym);
		break;
	case DT_BYTEARRAY:
	{
		int length = getLength(sym, DT_BYTEARRAY);
		for (int j = 0; j < length; j++)
		{
			char ch[3];
			sprintf_s(ch, 3, "%02x", ((char *)sym)[j] & 0xFF);
			str.append(ch);
		}
		break;
	}
	default:
		// shouldn't reach here
		return NULL;
	}
	return str;
}