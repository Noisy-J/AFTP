#pragma once
#ifndef TREE_H
#define TREE_H

#include <string>
#include <vector>
#include "ftp_client.h"

using namespace std;

void showTreeView(FTPClient& ftp, const vector<FTPFile>& files, const string& currentPath);

#endif