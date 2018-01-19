#pragma once
using namespace std;


BOOL findCSV(const char *file, string key, string value, string key2, vector<string> &row);
BOOL appendCSV(const char *file, vector<pair<string, string>> kvp);
BOOL replaceCSV(const char *file, string key, string value, vector<pair<string, string>> kvp);
