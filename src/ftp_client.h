#pragma once
#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <string>
#include <vector>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct FTPFile {
    string name;
    bool isDirectory;
    long long size;
};

class FTPClient {
private:
    SOCKET controlSocket;
    string server, username, password, currentDir;
    int port;
    bool connected, loggedIn;

    bool sendCommand(const string& cmd);
    string receiveResponse();
    string getPassiveMode();
    SOCKET createDataSocket();

public:
    FTPClient();
    ~FTPClient();

    bool connectToServer(const string& serverAddr, int portNum = 21);
    bool login(const string& user, const string& pass);
    void disconnect();
    void resetConnection();

    vector<FTPFile> listFiles();
    bool changeDirectory(const string& dir);
    string getCurrentDir() const { return currentDir; }
    string getServer() const { return server; }
    bool isConnected() const { return connected && loggedIn; }

    bool downloadFile(const string& remoteFile, const string& localFile);
    bool downloadFolder(const string& remoteFolder, const string& localFolder);
    bool uploadFile(const string& localFile, const string& remoteFile);
    bool uploadFolder(const string& localFolder, const string& remoteFolder);

    bool deleteFile(const string& filename);
    bool createDirectory(const string& dirname);
    bool renameFile(const string& oldName, const string& newName);
};

#endif