#pragma once
using namespace std;





class Binary
{
	UINT startAddr;
	UINT endAddr;
	char *binData;
	void clearData(void);

public:
	Binary()
	{
		binData = NULL;
	};
	~Binary()
	{
		delete[] binData;
	};
	void hex2bin(char *hexData, UINT start, UINT end);
	void merge(Binary &bin1, Binary &bin2);
	void add(char *data, UINT len, UINT addr, BOOL msb);
	UINT16 crc16(UINT start, UINT end);
	BOOL replace(char *find, char *replace, int len);
	int length(void)
	{
		if (binData == NULL)
			return 0;
		else
			return endAddr - startAddr + 1;
	}
	char* toCharArray(void)
	{
		return binData;
	}
};
