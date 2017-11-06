#include "stdafx.h"
#include "CmdProcessor.h"
#include "Resource.h"
#include "SymbolTable.h"
#include "Binary.h"
#include "Log.h"
#include "RunProcess.h"
using namespace std;

// Macros
#define CHECK_ARG_COUNT(n)		if(params.size() < n+1) { log("Incorrect no of args for: %s",params[0].c_str()); return FALSE; }
#define GETCHARPTR(a, b, c)		if ((b = (char *)sTable.getSymbol(a, c)) == NULL) b = (char *)a.c_str();
#define GETNUMBER(a, b)			if(!getNumber(a, b, sTable)) return FALSE;
#define GETBINARY(a, b)			if ((b = (Binary *)sTable.getSymbol(a, DT_BINARY)) == NULL) { log("Symbol %s not found",a.c_str()); return FALSE; }
#define GETUNKNOWN(a, b, c, d)		if(!getUnknown(a, &b, &c, d, sTable)) return FALSE;


BOOL tryStr2Num(string param, int *ptr)
{
	// if param starts with a number, then use it as a literal
	if ((param.at(0) >= '0' && param.at(0) <= '9') || param.at(0) == '+' || param.at(0) == '-')
	{
		*ptr = strtol(param.c_str(), NULL, 0);
		return TRUE;
	}
	return FALSE;
}

BOOL getNumber(string param, int &ptr, SymbolTable sTable)
{
	int *intPtr;

	if (tryStr2Num(param, &ptr))
		return TRUE;

	// check if param is an existing symbol
	if ((intPtr = (int *)sTable.getSymbol(param, DT_NUMBER)) == NULL)
	{
		log("Unknown symbol: %s", param.c_str());
		return FALSE;
	}

	ptr = *intPtr;

	return TRUE;
}

BOOL getUnknown(string &param, char **data, int *num, int &reqdType, SymbolTable sTable)
{
	int dataType = 0;
	*data = (char *)sTable.getSymbol(param, &dataType);
	if (*data == NULL)
	{
		// try number convertion first		
		if ((reqdType & DT_NUMBER) && tryStr2Num(param, num))
		{
			*data = (char *)num;
			dataType = DT_NUMBER;
		}
		else if(reqdType & DT_STRING) 
		{
			//treat as string literal
			*data = (char *)param.c_str();
			dataType = DT_STRING;
		}
	}
	if (dataType == 0)
	{
		log("Unknown symbol: %s", param.c_str());
		return FALSE;
	}
	// check if requested data type
	if (!(dataType & reqdType))
	{
		log("Invalid data type : %s", param.c_str());
		return FALSE;
	}
	reqdType = dataType;
	return TRUE;
}

BOOL cf_hex2bin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(4);
	// destination
	Binary *bin = (Binary *)sTable.newSymbol(params[1], DT_BINARY);
	// source file
	char *src;
	GETCHARPTR(params[2], src, DT_STRING);
	// start address
	int startAddr;
	GETNUMBER(params[3], startAddr);
	// end address
	int endAddr;
	GETNUMBER(params[4], endAddr);

	// load the hex file
	HANDLE hFile = CreateFile(src, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		logE("Unable to open file %s", src);
		return FALSE;
	}
	DWORD readFileLen = GetFileSize(hFile, NULL);
	DWORD NumberOfBytesRead;
	CHAR *buffer = new CHAR[readFileLen];
	if (!ReadFile(hFile, buffer, readFileLen, &NumberOfBytesRead, NULL))
	{
		logE("Unable to read file %s", src);
		CloseHandle(hFile);
		return FALSE;
	}
	CloseHandle(hFile);
	// convert to binary
	log("Converting %s to binary", src);
	try 
	{
		bin->hex2bin(buffer, startAddr, endAddr);
	}
	catch (const char *err) 
	{ 
		log(err); 
		delete[] buffer;
		return FALSE; 
	}
	// free the buffer
	delete[] buffer;

	return TRUE;
}

BOOL cf_mergebin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(3);
	// destination
	Binary *bin = (Binary *)sTable.addSymbol(params[1], DT_BINARY);
	// source 1
	Binary *bin1;
	GETBINARY(params[2], bin1);
	// source 2
	Binary *bin2;
	GETBINARY(params[3], bin2);

	log("Merging %s and %s ...", params[2].c_str(), params[3].c_str());
	bin->merge(*bin1, *bin2);

	return TRUE;
}

BOOL cf_add2bin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(5);
	// destination
	Binary *bin = (Binary *)sTable.addSymbol(params[1], DT_BINARY);
	// data
	int num; // reqd by getunknown
	int dataType = DT_NUMBER | DT_STRING | DT_BYTEARRAY;
	char *data;
	GETUNKNOWN(params[2], data, num, dataType);
	// address
	int addr_s;
	GETNUMBER(params[3], addr_s);
	UINT addr = (UINT)addr_s;
	// length
	int len_s;
	GETNUMBER(params[4], len_s);
	UINT len = (UINT)len_s;
	// msb/lsb
	BOOL msb = FALSE;
	if (params[5] == "msb") msb = TRUE;
	else if (params[5] != "lsb") 
	{
		log("Invalid argument : %s", params[5].c_str()); 
		return FALSE;
	}
	// check bounds
	UINT checkLen = sTable.getLength(data, dataType);
	if (len > checkLen)
	{
		log("Invalid length for : %s", params[2].c_str());
		return FALSE;
	}

	// add 2 bin
	log("Adding %s to %s", params[2].c_str(), params[1].c_str());
	bin->add(data, len, addr, msb);
	return TRUE;
}

BOOL cf_replacebin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(3);
	// destination
	Binary *bin;
	GETBINARY(params[1], bin);

	// find
	int num;
	int dataType = DT_STRING | DT_BYTEARRAY;
	char *find;
	GETUNKNOWN(params[2], find, num, dataType);
	int flen = sTable.getLength(find, dataType);
	// shouldn't copy terminating null char if string
	if (dataType == DT_STRING) flen--;

	// replace
	dataType = DT_STRING | DT_BYTEARRAY;
	char *replace;
	GETUNKNOWN(params[3], replace, num, dataType);
	int rlen = sTable.getLength(replace, dataType);
	// shouldn't copy terminating null char if string
	if (dataType == DT_STRING) rlen--;

	// lengths of find & replace should be equal
	if (flen != rlen)
	{
		log("Find and replace lengths must be equal");
		return FALSE;
	}

	log("Replacing %s with %s ...", params[2].c_str(), params[3].c_str());
	if (!bin->replace(find, replace, flen))
	{
		log("Could not find %s in %s", params[2].c_str(), params[1].c_str());
		return FALSE;
	}
	return TRUE;
}

BOOL cf_crc16bin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(4);
	// destination
	int *crc = (int *)sTable.newSymbol(params[1], DT_NUMBER);
	// source file
	Binary *bin;
	GETBINARY(params[2], bin);
	// start address
	int startAddr;
	GETNUMBER(params[3], startAddr);
	// end address
	int endAddr;
	GETNUMBER(params[4], endAddr);

	log("Calculating crc16 on %s", params[2].c_str());
	*crc = bin->crc16(startAddr, endAddr);

	return TRUE;
}

BOOL cf_savebin(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(2);
	// destination
	char *saveFile;
	GETCHARPTR(params[1], saveFile, DT_STRING);
	// source
	Binary *bin;
	GETBINARY(params[2], bin);

	char *data = bin->toCharArray();
	if (data == NULL)
	{
		log("Symbol %s is empty", params[2].c_str());
		return FALSE;
	}
	HANDLE hFile = CreateFile(saveFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		logE("Unable to create file %s", saveFile);
		return FALSE;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) log("Overwriting existing file %s", saveFile);
	DWORD nBytesWritten;
	if (!WriteFile(hFile, data, bin->length(), &nBytesWritten, NULL))
	{
		logE("Unable to write to file %s", saveFile);
		return FALSE;
	}
	CloseHandle(hFile);
	log("%s saved to %s", params[2].c_str(), saveFile);
	return TRUE;
}

BOOL cf_run(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(2);
	// program
	char *runFile;
	GETCHARPTR(params[1], runFile, DT_STRING);
	// cmd line
	char *cmdLine;
	GETCHARPTR(params[2], cmdLine, DT_STRING);
	// check for optional arguments to store the output
	BOOL storeOutput = FALSE;
	string output;
	if (params.size() > 3)
	{
		storeOutput = TRUE;
	}

	log("Launching %s ...", PathFindFileName(runFile));
	BOOL result = runProcess(runFile, cmdLine, state, storeOutput?&output:NULL);
	if (!storeOutput || !result) return result;

	// create a new symbol & store the output
	char *str = (char *)sTable.newSymbol(params[3], DT_STRING, output.length() + 1);
	memcpy(str, output.c_str(), output.length() + 1);
	return TRUE;
}

BOOL cf_substring(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(4);
	// destination
	// source
	char *srcstr;
	GETCHARPTR(params[2], srcstr, DT_STRING);
	// offset
	int offset;
	GETNUMBER(params[3], offset);
	// length
	int len;
	GETNUMBER(params[4], len);

	string str(srcstr);
	// if offset is -ve, offset from the end
	if (offset < 0) offset = str.length() + offset;
	// check bounds
	if (len < 1 || (offset + len) > (int)str.length())
	{
		log("Substring parameters out of bounds for %s", params[2].c_str());
		return FALSE;
	}
	log("Extracting substring from %s ...", params[2].c_str());
	str = str.substr(offset, len);
	// copy to destination
	char *dststr = (char *)sTable.newSymbol(params[1], DT_STRING, str.length() + 1);
	memcpy(dststr, str.c_str(), str.length() + 1);
	return TRUE;
}

BOOL cf_replace(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(4);
	// destination
	// source
	char *srcstr;
	GETCHARPTR(params[2], srcstr, DT_STRING);
	// find
	char *find;
	GETCHARPTR(params[3], find, DT_STRING);
	// replace with
	char *replace;
	GETCHARPTR(params[4], replace, DT_STRING);

	log("Replacing characters in %s ...", params[2].c_str());
	string str(srcstr);
	string f(find);
	string r(replace);
	int n;
	// replace all
	while ((n = str.find(f)) != string::npos)
	{
		str.replace(n, f.length(), r);
	}
	// copy to destination
	char *dststr = (char *)sTable.newSymbol(params[1], DT_STRING, str.length() + 1);
	memcpy(dststr, str.c_str(), str.length() + 1);
	return TRUE;
}

BOOL cf_length(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(2);
	// destination
	// source
	int dataType;
	void *sym = sTable.getSymbol(params[2], &dataType);
	if(sym == NULL)
	{
		log("Symbol %s not found", params[2].c_str()); return FALSE;
	}
	// get length
	int length = sTable.getLength(sym, dataType);
	
	// store it
	int *lenptr = (int *)sTable.newSymbol(params[1], DT_NUMBER);
	*lenptr = length;
	log("Stored length of %s in %s", params[2].c_str(), params[1].c_str());
	return TRUE;
}

BOOL genRand(BYTE *buffer, int length)
{
	HCRYPTPROV hProvider = NULL;

	if (!CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
	{
		logE("Failed to initialize cryptographic provider");
		return FALSE;
	}

	if (!CryptGenRandom(hProvider, length, buffer))
	{
		CryptReleaseContext(hProvider, 0);
		logE("Failed to generate random number");
		return FALSE;
	}
	
	CryptReleaseContext(hProvider, 0);
	return TRUE;
}

BOOL cf_rand(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(2);
	// destination
	// length
	int length;
	GETNUMBER(params[2], length);

	// create destination byte array
	char *dst = (char *)sTable.newSymbol(params[1], DT_BYTEARRAY, length);
	
	genRand((BYTE *)dst, length);
	log("Random bytes stored in %s", params[1].c_str());
	return TRUE;
}

BOOL cf_randtable(vector<string> params, SymbolTable &sTable, State &state)
{
	CHECK_ARG_COUNT(1);
	// seed the RNG
	UINT seed;
	if (!genRand((BYTE *)&seed, sizeof(seed))) return FALSE;
	srand(seed);

	// populate buffer with 0 - 255
	std::vector<BYTE> buffer;
	for (int i = 0; i < 256; i++) buffer.push_back(i);

	// shuffle
	random_shuffle(buffer.begin(), buffer.end());
	
	// copy to destination
	char *dst = (char *)sTable.newSymbol(params[1], DT_BYTEARRAY, 256);
	for (auto it : buffer) *dst++ = it;
	log("Random number table stored in %s", params[1].c_str());
	return TRUE;
}

