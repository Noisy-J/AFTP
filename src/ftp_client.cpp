#include "ftp_client.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

FTPClient::FTPClient() : controlSocket(INVALID_SOCKET), connected(false), loggedIn(false),
currentDir("/"), port(21) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

FTPClient::~FTPClient() { disconnect(); WSACleanup(); }

bool FTPClient::sendCommand(const string& cmd) {
    string command = cmd + "\r\n";
    return send(controlSocket, command.c_str(), (int)command.length(), 0) != SOCKET_ERROR;
}

string FTPClient::receiveResponse() {
    char buffer[4096];
    string response;
    int bytes;
    do {
        bytes = recv(controlSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        response += buffer;
    } while (bytes == sizeof(buffer) - 1);
    return response;
}

bool FTPClient::connectToServer(const string& serverAddr, int portNum) {
    if (controlSocket != INVALID_SOCKET) closesocket(controlSocket);

    server = serverAddr;
    port = portNum;
    controlSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (controlSocket == INVALID_SOCKET) return false;

    hostent* host = gethostbyname(server.c_str());
    if (!host) return false;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);

    if (connect(controlSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(controlSocket);
        controlSocket = INVALID_SOCKET;
        return false;
    }

    receiveResponse();
    connected = true;
    return true;
}

bool FTPClient::login(const string& user, const string& pass) {
    if (!connected) return false;
    sendCommand("USER " + user);
    receiveResponse();
    sendCommand("PASS " + pass);
    string response = receiveResponse();

    if (response.find("230") != string::npos) {
        username = user;
        password = pass;
        loggedIn = true;

        sendCommand("PWD");
        response = receiveResponse();
        size_t start = response.find('"');
        size_t end = response.rfind('"');
        if (start != string::npos && end != string::npos) {
            currentDir = response.substr(start + 1, end - start - 1);
        }
        return true;
    }
    return false;
}

string FTPClient::getPassiveMode() {
    sendCommand("PASV");
    string response = receiveResponse();
    size_t start = response.find('(');
    size_t end = response.find(')');
    if (start == string::npos || end == string::npos) return "";

    string nums = response.substr(start + 1, end - start - 1);
    replace(nums.begin(), nums.end(), ',', ' ');
    int h1, h2, h3, h4, p1, p2;
    stringstream ss(nums);
    ss >> h1 >> h2 >> h3 >> h4 >> p1 >> p2;
    int dataPort = p1 * 256 + p2;
    return to_string(h1) + "." + to_string(h2) + "." + to_string(h3) + "." + to_string(h4) + ":" + to_string(dataPort);
}

SOCKET FTPClient::createDataSocket() {
    string passive = getPassiveMode();
    if (passive.empty()) return INVALID_SOCKET;

    size_t colon = passive.find(':');
    string host = passive.substr(0, colon);
    int portNum = stoi(passive.substr(colon + 1));

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portNum);
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

vector<FTPFile> FTPClient::listFiles() {
    vector<FTPFile> files;

    SOCKET dataSocket = createDataSocket();
    if (dataSocket == INVALID_SOCKET) return files;

    sendCommand("LIST");
    receiveResponse();

    char buffer[65536];
    string data;
    int bytes;
    while ((bytes = recv(dataSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        data += buffer;
    }

    closesocket(dataSocket);
    receiveResponse();

    stringstream ss(data);
    string line;
    while (getline(ss, line)) {
        if (line.empty()) continue;

        FTPFile file;
        file.isDirectory = (line[0] == 'd');
        file.size = 0;

        size_t lastSpace = line.find_last_of(' ');
        if (lastSpace != string::npos) {
            file.name = line.substr(lastSpace + 1);
            while (!file.name.empty() && (file.name.back() == '\r' || file.name.back() == '\n')) {
                file.name.pop_back();
            }
            if (!file.name.empty() && file.name != "." && file.name != "..") {
                files.push_back(file);
            }
        }
    }

    return files;
}

bool FTPClient::changeDirectory(const string& dir) {
    sendCommand("CWD " + dir);
    string response = receiveResponse();
    if (response.find("250") != string::npos) {
        if (dir == "..") {
            size_t lastSlash = currentDir.find_last_of('/');
            if (lastSlash > 0) currentDir = currentDir.substr(0, lastSlash);
            else currentDir = "/";
        }
        else if (dir[0] == '/') {
            currentDir = dir;
        }
        else {
            if (currentDir != "/") currentDir += "/" + dir;
            else currentDir += dir;
        }
        return true;
    }
    return false;
}

bool FTPClient::downloadFile(const string& remoteFile, const string& localFile) {
    SOCKET dataSocket = createDataSocket();
    if (dataSocket == INVALID_SOCKET) return false;

    sendCommand("RETR " + remoteFile);
    string response = receiveResponse();
    if (response.find("150") == string::npos && response.find("125") == string::npos) {
        closesocket(dataSocket);
        return false;
    }

    ofstream file(localFile, ios::binary);
    if (!file.is_open()) {
        closesocket(dataSocket);
        return false;
    }

    char buffer[8192];
    int bytes;
    while ((bytes = recv(dataSocket, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytes);
    }

    file.close();
    closesocket(dataSocket);
    receiveResponse();
    return true;
}

bool FTPClient::uploadFile(const string& localFile, const string& remoteFile) {
    ifstream file(localFile, ios::binary);
    if (!file.is_open()) return false;

    SOCKET dataSocket = createDataSocket();
    if (dataSocket == INVALID_SOCKET) {
        file.close();
        return false;
    }

    sendCommand("STOR " + remoteFile);
    string response = receiveResponse();
    if (response.find("150") == string::npos && response.find("125") == string::npos) {
        closesocket(dataSocket);
        file.close();
        return false;
    }

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        int bytes = (int)file.gcount();
        if (send(dataSocket, buffer, bytes, 0) == SOCKET_ERROR) break;
    }

    file.close();
    closesocket(dataSocket);
    receiveResponse();
    return true;
}

bool FTPClient::downloadFolder(const string& remoteFolder, const string& localFolder) {
    if (!fs::exists(localFolder)) {
        fs::create_directories(localFolder);
    }

    string oldDir = currentDir;
    if (!changeDirectory(remoteFolder)) {
        changeDirectory(oldDir);
        return false;
    }

    auto files = listFiles();
    for (const auto& file : files) {
        string localPath = localFolder + "/" + file.name;
        if (file.isDirectory) {
            downloadFolder(file.name, localPath);
        }
        else {
            downloadFile(file.name, localPath);
        }
    }

    changeDirectory(oldDir);
    return true;
}

bool FTPClient::uploadFolder(const string& localFolder, const string& remoteFolder) {
    createDirectory(remoteFolder);

    string oldDir = currentDir;
    changeDirectory(remoteFolder);

    for (const auto& entry : fs::directory_iterator(localFolder)) {
        string name = entry.path().filename().string();
        if (fs::is_directory(entry.path())) {
            uploadFolder(entry.path().string(), name);
        }
        else {
            uploadFile(entry.path().string(), name);
        }
    }

    changeDirectory(oldDir);
    return true;
}

bool FTPClient::deleteFile(const string& filename) {
    sendCommand("DELE " + filename);
    return receiveResponse().find("250") != string::npos;
}

bool FTPClient::createDirectory(const string& dirname) {
    sendCommand("MKD " + dirname);
    return receiveResponse().find("257") != string::npos;
}

bool FTPClient::renameFile(const string& oldName, const string& newName) {
    sendCommand("RNFR " + oldName);
    if (receiveResponse().find("350") == string::npos) return false;
    sendCommand("RNTO " + newName);
    return receiveResponse().find("250") != string::npos;
}

void FTPClient::resetConnection() {
    if (controlSocket != INVALID_SOCKET) closesocket(controlSocket);
    controlSocket = INVALID_SOCKET;
    connected = loggedIn = false;
    currentDir = "/";
}

void FTPClient::disconnect() {
    if (connected && controlSocket != INVALID_SOCKET) {
        sendCommand("QUIT");
        receiveResponse();
        closesocket(controlSocket);
        controlSocket = INVALID_SOCKET;
        connected = loggedIn = false;
    }
}