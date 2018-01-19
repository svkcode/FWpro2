#include "stdafx.h"
#include "Log.h"
#include "CSVParser.h"
using namespace std;

BOOL openCSV(const char *fileName, HANDLE &hFile)
{
	hFile = CreateFile(fileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		logE("Unable to open file %s", fileName);
		return FALSE;
	}
	return TRUE;
}

BOOL readLineCSV(HANDLE hFile, vector<string> &row, DWORD &filePointer)
{
	TCHAR buffer[256];
	DWORD nBytesRead = 1;	
	string line;
	UINT n;
	row.clear();
	// get pointer to current line
	filePointer = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
	// read current line
	while (nBytesRead != 0)
	{
		if (!ReadFile(hFile, buffer, sizeof(buffer), &nBytesRead, NULL))
		{
			logE("Unable to read csv file");
			return FALSE;
		}
		if (nBytesRead > 0)
		{
			line += string(buffer, nBytesRead);
			// ignoring embedded line breaks
			if ((n = line.find("\n")) != string::npos)
				break;
		}
	}
	// EOF
	if (line.empty()) return TRUE;

	// get substring of one line
	if (n != string::npos)
	{		
		// set file pointer to beginning of next line
		if (SetFilePointer(hFile, n - line.size() + 1, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		{
			logE("Unable to set csv file pointer");
			return FALSE;
		}
		line = line.substr(0, n);
	}

	// remove CR
	if (line.back() == '\r') line.pop_back();
	n = 0;
	// split line by commas
	while ((n = line.find_first_of(",\"", n)) != string::npos)
	{
		if (line.at(n) == '"')				// double quote
		{
			// erase the quote
			line.erase(n, 1);
			while (true)
			{
				// find the next quote
				n = line.find("\"", n);
				if (n == string::npos) goto EOL;
				// last character
				if (n == line.size() - 1)
				{
					line.pop_back();
					goto EOL;
				}
				// escaped quote
				if (line.at(n + 1) == '\"')
				{
					// leave one quote n search again
					line.erase(n, 1);
					n++;
					continue;
				}
				// found, erase it
				line.erase(n, 1);
				break;
			}
			// find comma
			continue;
		}

		
		// add to vector
		row.push_back(line.substr(0, n));
		line.erase(0, n + 1);
		n = 0;
	}
	EOL:
	row.push_back(line.substr(0, n));
	return TRUE;
}

int getColumnIdx(vector<string> header, string key)
{
	for (UINT i = 0; i < header.size(); i++)
	{
		if (header.at(i) == key)
		{
			return i;
		}
	}
	log("%s is not a valid header in the csv file", key.c_str());
	return -1;
}

BOOL findCSV(HANDLE hFile, vector<string> header, string key, string value, vector<string> &row, DWORD &filePointer)
{
	int column;
	// find column index of the key
	if((column = getColumnIdx(header, key)) == -1)	
		return FALSE;

	while (true)
	{
		if (!readLineCSV(hFile, row, filePointer)) return FALSE; // Error
		if (row.size() == 0) return TRUE; // EOF
		if (row.size() > (UINT)column)
		{
			if (row.at(column) == value)
				return TRUE;
		}		
	}
}

string compileRow(vector<string> header, vector<pair<string, string>> kvp)
{
	string row;
	UINT count = kvp.size();
	// add values to the corresponding columns based on their keys
	for (auto ith : header)
	{
		for (auto itk : kvp)
		{
			if (ith == itk.first)
			{
				row.append(itk.second);
				count--;
				break;
			}
		}
		row.append(",");
	}
	// one less comma than columns
	row.pop_back();
	row.append("\r\n");

	// if not all keys were found in the header
	if (count > 0) log("Ignoring %d key/value pair(s) not found in the header", count);

	return row;
}

BOOL appendCSV(HANDLE hFile, string row)
{
	DWORD nBytesWritten;

	// prepend new line if not already present
	if (SetFilePointer(hFile, -1, NULL, FILE_END) == INVALID_SET_FILE_POINTER)
	{
		logE("Unable to set csv file pointer");
		return FALSE;
	}
	TCHAR lastChar;
	if (!ReadFile(hFile, &lastChar, 1, &nBytesWritten, NULL))
	{
		logE("Unable to read csv file");
		return FALSE;
	}
	if (lastChar != '\n') row = "\r\n" + row;

	if (!WriteFile(hFile, row.c_str(), row.size(), &nBytesWritten, NULL))
	{
		logE("Unable to write csv file");
		return FALSE;
	}
	return TRUE;
}

BOOL replaceCSV(HANDLE hFile, string row, DWORD writePtr)
{
	// assuming pointer is in the line following the line to replace
	DWORD readPtr = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);

	// buffers to hold read & write data
	UINT size = 1024;
	if (row.size() > size) size = row.size();
	TCHAR *buf1 = new TCHAR[size];
	TCHAR *buf2 = new TCHAR[size];
	TCHAR *bufPtr[] = { buf1, buf2 };
	DWORD bufLen[] = { row.size(), 0 };
	memcpy(buf1, row.c_str(), bufLen[0]);

	UINT index = 1;
	DWORD nBytesWritten;
	while (bufLen[index ^ 1])	// while there are bytes to write
	{
		// set pointer at data to read
		if (SetFilePointer(hFile, readPtr, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			logE("Unable to set csv file pointer");
			goto error;
		}
		// read data
		if (!ReadFile(hFile, bufPtr[index], size, &bufLen[index], NULL))
		{
			logE("Unable to read csv file");
			goto error;
		}
		// update read ptr
		readPtr += bufLen[index];
		// change buffer
		index ^= 1;
		// set pointer at data to overwrite
		if (SetFilePointer(hFile, writePtr, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			logE("Unable to set csv file pointer");
			goto error;
		}
		// write data
		if (!WriteFile(hFile, bufPtr[index], bufLen[index], &nBytesWritten, NULL))
		{
			logE("Unable to write csv file");
			goto error;
		}
		// update write ptr
		writePtr += nBytesWritten;
	}
	// update end of file
	if (!SetEndOfFile(hFile))
	{
		logE("Unable to update csv file size");
		goto error;
	}
	delete[] buf1;
	delete[] buf2;
	return TRUE;
error:
	delete[] buf1;
	delete[] buf2;
	return FALSE;
}

BOOL findCSV(const char *file, string key, string value, string key2, vector<string> &row)
{
	HANDLE hFile;
	vector<string> header;
	DWORD filePtr;
	int column;
	// open
	if (!openCSV(file, hFile)) return FALSE;
	// read header
	if (!readLineCSV(hFile, header, filePtr)) goto error;
	// find
	if (!findCSV(hFile, header, key, value, row, filePtr)) goto error;
	CloseHandle(hFile);
	// return entire row if no specific key requested
	if (key2 == "") return TRUE;	
	// check if key/value exists
	if (row.empty())
	{
		log("%s : %s does not exist in %s", key.c_str(), value.c_str(), file);
		return FALSE;
	}
	// find column index of the key
	if ((column = getColumnIdx(header, key2)) == -1)
		return FALSE;
	// move the value to the first position & resize
	row.at(0) = row.at(column);
	row.resize(1);

	return TRUE;

error:
	CloseHandle(hFile);
	return FALSE;
}

BOOL appendCSV(const char *file, vector<pair<string, string>> kvp)
{
	HANDLE hFile;
	vector<string> header;
	DWORD filePtr;
	string row;
	// open
	if (!openCSV(file, hFile)) return FALSE;
	// read header
	if (!readLineCSV(hFile, header, filePtr)) goto error;
	// compile key/value pairs into a csv line
	row = compileRow(header, kvp);
	if (!appendCSV(hFile, row)) goto error;

	CloseHandle(hFile);
	return TRUE;

error:
	CloseHandle(hFile);
	return FALSE;
}

BOOL replaceCSV(const char *file, string key, string value, vector<pair<string, string>> kvp)
{
	HANDLE hFile;
	vector<string> header;
	DWORD filePtr;
	vector<string> temp;
	string row;
	// open
	if (!openCSV(file, hFile)) return FALSE;
	// read header
	if (!readLineCSV(hFile, header, filePtr)) goto error;	
	// find existing if any
	if (!findCSV(hFile, header, key, value, temp, filePtr)) goto error;
	// compile key/value pairs into a csv line
	row = compileRow(header, kvp);
	if (!temp.empty())
	{
		log("Replacing existing record %s : %s", key.c_str(), value.c_str());
		if (!replaceCSV(hFile, row, filePtr)) goto error;
	}
	else
	{
		if (!appendCSV(hFile, row)) goto error;
	}

	CloseHandle(hFile);
	return TRUE;

error:
	CloseHandle(hFile);
	return FALSE;
}