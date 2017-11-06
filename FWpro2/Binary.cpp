#include "stdafx.h"
#include "Binary.h"

void Binary::clearData(void)
{
	if (binData != NULL)
	{
		delete[] binData;
		binData = NULL;
	}
}

void Binary::hex2bin(char *hexData, UINT start, UINT end)
{
	char *ptr;
	char *context;
	// clear binData
	clearData();
	int binLen = end - start + 1;
	char *temp = new char[binLen];
	// fill temp with 0xFF (flash mem default)
	for (int i = 0; i < binLen; i++) temp[i] = (char)0xFF;
	UINT addr = 0;
	binLen = 0;
	// stores the actual start & end addresses
	startAddr = end, endAddr = start;
	// move ptr to next line
	ptr = strtok_s(hexData, ":\r\n", &context);
	// process line by line
	while (ptr != NULL)
	{
		string line(ptr);
		// record type byte
		string type = line.substr(6, 2);
		// length byte
		int len = stoi(line.substr(0, 2), NULL, 16);
		if (type == "00")			// data
		{
			// update the low word
			addr = (addr & 0xFFFF0000) | stoi(line.substr(2, 4), NULL, 16);

			for (int i = 0; i < len*2; i += 2)
			{
				// check if the current address is within the requested bounds
				if (addr >= start && addr <= end)
				{
					// update the actual start & end addresses
					if (addr < startAddr) startAddr = addr;
					if (addr > endAddr) endAddr = addr;
					// store the byte at offset address
					temp[addr - start] = stoi(line.substr(8 + i, 2), NULL, 16);
					binLen++;
				}
				addr++;
			}
		}
		else if (type == "04")		// extended linear address
		{
			// update the high word
			addr = (addr & 0xFFFF) | (stoi(line.substr(8, 4), NULL, 16) << 16);
		}
		else if (type == "01")		// end of file
		{
			break;
		}
		else
		{
			delete[] temp;
			throw "Unknown record type encountered";
		}
		// move ptr to next line
		ptr = strtok_s(NULL, ":\r\n", &context);
	}
	// copy to binData
	if (binLen)
	{
		binLen = endAddr - startAddr + 1;
		binData = new char[binLen];
		for (int i = 0, j = startAddr - start; i < binLen; i++, j++)
			binData[i] = temp[j];
	}
	// free the buffer
	delete[] temp;
}

void Binary::merge(Binary &bin1, Binary &bin2)
{
	char *temp = NULL;
	int len = 0;
	UINT start = 0xFFFFFFFF;
	UINT end = 0;
	// check if either sources are empty & update the start & end addresses
	if (bin1.binData != NULL)
	{
		if (bin1.startAddr < start) start = bin1.startAddr;
		if (bin1.endAddr > end) end = bin1.endAddr;
		temp = bin1.binData;
	}
	if (bin2.binData != NULL)
	{
		if (bin2.startAddr < start) start = bin2.startAddr;
		if (bin2.endAddr > end) end = bin2.endAddr;
		if (temp != NULL) goto do_merge;
		temp = bin2.binData;
	}
	if (temp == NULL)
	{
		clearData();
		return;
	}
	else goto copy;

do_merge:
	len = end - start + 1;
	temp = new char[len];
	// fill temp with 0xFF (flash mem default)
	for (int i = 0; i < len; i++) temp[i] = (char)0xFF;
	// copy bin1
	for (int i = 0, j = bin1.startAddr - start; i < bin1.length(); i++, j++)
		temp[j] = bin1.binData[i];
	// copy bin2
	for (int i = 0, j = bin2.startAddr - start; i < bin2.length(); i++, j++)
		temp[j] = bin2.binData[i];

copy:
	clearData();
	int binLen = end - start + 1;
	binData = new char[binLen];
	for (int i = 0; i < binLen; i++)
		binData[i] = temp[i];
	startAddr = start;
	endAddr = end;

	// free buffer
	if (len) delete[] temp;

}

void Binary::add(char *data, UINT len, UINT addr, BOOL msb)
{
	UINT start = addr;
	UINT end = start + len - 1;
	if (binData == NULL)
	{
		binData = new char[len];
		startAddr = start;
		endAddr = end;
	}
	else
	{
		// get the lowest start & highest end
		if (start > startAddr) start = startAddr;
		if (end < endAddr) end = endAddr;
		// need new heap if binary needs to grow
		if (start < startAddr || end > endAddr)
		{
			int nlen = end - start + 1;
			char *temp = new char[nlen];
			// fill with 0xFF
			for (int i = 0; i < nlen; i++) temp[i] = (char)0xFF;
			// copy from binData
			for (int i = 0; i < length(); i++)
					temp[i + (startAddr - start)] = binData[i];

			delete[] binData;
			binData = temp;
			startAddr = start;
			endAddr = end;
		}
	}
	// copy data to binary
	if (msb)
	{
		for (UINT i = 0; i < len; i++)
			binData[i + addr - startAddr] = data[len - 1 - i];
	}
	else
	{
		for (UINT i = 0; i < len; i++)
			binData[i + addr - startAddr] = data[i];
	}
}

UINT16 Binary::crc16(UINT start, UINT end)
{
	if (binData == NULL) return 0;

	// adjust start & end addresses
	if (start < startAddr) start = startAddr;
	if (end > endAddr) end = endAddr;

	UINT16 crc = 0xFFFF;
	for (UINT i = start; i < end; i++)
	{
		crc = crc ^ (binData[i - startAddr] << 8);
		for (int j = 0; j < 8; j++)
		{
			if ((crc & 0x8000) == 0x8000)
			{
				crc = crc << 1;
				crc ^= 0x1021;
			}
			else
			{
				crc = crc << 1;
			}
		}
	}
	return crc;
}

BOOL Binary::replace(char *find, char *replace, int len)
{
	if (binData == NULL) return FALSE;

	// find index of match
	int i;
	for (i = 0; i <= (length() - len); i++)
	{
		int j;
		for (j = 0; j < len; j++)
		{
			if (binData[i + j] != find[j]) break;
		}
		if (j == len)
			goto found;
	}
	return FALSE;

found:
	// replace
	for (int j = 0; j < len; j++)
		binData[i++] = replace[j];
	return TRUE;
}