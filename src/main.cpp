// AFTP - FTP Client
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <conio.h>
#include <windows.h>
#include <ctime>
#include <filesystem>
#include <map>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

// ==================== ЦВЕТА ====================
#define COLOR_DEFAULT 7
#define COLOR_GREEN 10
#define COLOR_RED 12
#define COLOR_YELLOW 14
#define COLOR_CYAN 11
#define COLOR_WHITE 15

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void setRussianConsole() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    setlocale(LC_ALL, "Russian");
    system("chcp 1251 > nul");
}

void hideCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
}

void showCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
}

void showCursorAt(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    showCursor();
}

// Функция для скрытия курсора после ввода
void hideCursorAfterInput() {
    hideCursor();
}

// ==================== НАСТРОЙКИ ====================
string currentLanguage = "ru";
map<string, string> langStrings;

void clearScreen() {
    system("cls");
}

void loadLanguage(const string& lang) {
    langStrings.clear();
    string langFile = "lang/" + lang + ".lang";
    ifstream file(langFile);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t pos = line.find('=');
            if (pos != string::npos) {
                langStrings[line.substr(0, pos)] = line.substr(pos + 1);
            }
        }
        file.close();
    }
    currentLanguage = lang;
}

string _(const string& key) {
    if (langStrings.find(key) != langStrings.end()) return langStrings[key];
    return key;
}

void loadAllSettings() {
    ifstream file("aftp_settings.ini");
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != string::npos) {
                string key = line.substr(0, pos);
                string val = line.substr(pos + 1);
                if (key == "language") loadLanguage(val);
            }
        }
        file.close();
    }
}

void saveAllSettings() {
    ofstream file("aftp_settings.ini");
    if (file.is_open()) {
        file << "language=" << currentLanguage << "\n";
        file.close();
    }
}

// ==================== СТРУКТУРА ПРОФИЛЯ ====================
struct FTPProfile {
    string name;
    string server;
    string port;
    string username;
    string password;

    bool isValid() const {
        return !name.empty() && !server.empty() && !port.empty() &&
            !username.empty() && !password.empty();
    }
};

// ==================== УПРАВЛЕНИЕ ПРОФИЛЯМИ ====================
vector<FTPProfile> profiles;
string profilesFile = "ftp_profiles.dat";

void loadProfiles() {
    profiles.clear();
    ifstream file(profilesFile);
    if (file.is_open()) {
        FTPProfile profile;
        string line;
        while (getline(file, line)) {
            if (line == "[PROFILE]") {
                if (!profile.name.empty()) {
                    profiles.push_back(profile);
                    profile = FTPProfile();
                }
            }
            else if (line.find("NAME=") == 0) profile.name = line.substr(5);
            else if (line.find("SERVER=") == 0) profile.server = line.substr(7);
            else if (line.find("PORT=") == 0) profile.port = line.substr(5);
            else if (line.find("USERNAME=") == 0) profile.username = line.substr(9);
            else if (line.find("PASSWORD=") == 0) profile.password = line.substr(9);
        }
        if (!profile.name.empty()) profiles.push_back(profile);
        file.close();
    }
}

void saveProfiles() {
    ofstream file(profilesFile);
    if (file.is_open()) {
        for (const auto& profile : profiles) {
            file << "[PROFILE]\n";
            file << "NAME=" << profile.name << "\n";
            file << "SERVER=" << profile.server << "\n";
            file << "PORT=" << profile.port << "\n";
            file << "USERNAME=" << profile.username << "\n";
            file << "PASSWORD=" << profile.password << "\n";
        }
        file.close();
    }
}

// ==================== ИСТОРИЯ ====================
vector<string> historyList;

void addToHistory(const string& action) {
    time_t now = time(0);
    char* dt = ctime(&now);
    string timestamp(dt);
    timestamp.pop_back();
    historyList.push_back("[" + timestamp + "] " + action);

    ofstream file("ftp_history.txt", ios::app);
    if (file.is_open()) {
        file << "[" << timestamp << "] " << action << endl;
        file.close();
    }
    if (historyList.size() > 100) historyList.erase(historyList.begin());
}

void showHistory() {
    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                           " << _("history") << "                                 |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);

    if (historyList.empty()) cout << "| " << _("empty") << string(70, ' ') << " |\n";
    else for (const auto& entry : historyList) {
        cout << "| " << entry;
        if (entry.length() < 76) cout << string(76 - entry.length(), ' ');
        cout << " |\n";
    }

    cout << "+============================================================================+\n";
    cout << "| [Any key] " << _("back") << "\n";
    cout << "+============================================================================+\n";
    _getch();
}

// ==================== FTP КЛИЕНТ ====================
class FTPClient {
private:
    SOCKET controlSocket;
    string server, username, password, currentRemoteDir;
    int port;
    bool connected, loggedIn;

    bool sendCommand(const string& cmd) {
        string command = cmd + "\r\n";
        return send(controlSocket, command.c_str(), (int)command.length(), 0) != SOCKET_ERROR;
    }

    string receiveResponse() {
        char buffer[4096];
        string response;
        int bytesReceived;
        do {
            bytesReceived = recv(controlSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) break;
            buffer[bytesReceived] = '\0';
            response += buffer;
        } while (bytesReceived == (int)(sizeof(buffer) - 1));
        return response;
    }

    string getPassiveMode() {
        sendCommand("PASV");
        string response = receiveResponse();
        size_t start = response.find('(');
        size_t end = response.find(')');
        if (start == string::npos || end == string::npos) return "";

        string numbers = response.substr(start + 1, end - start - 1);
        replace(numbers.begin(), numbers.end(), ',', ' ');
        istringstream iss(numbers);
        int h1, h2, h3, h4, p1, p2;
        iss >> h1 >> h2 >> h3 >> h4 >> p1 >> p2;
        int dataPort = p1 * 256 + p2;
        return to_string(h1) + "." + to_string(h2) + "." + to_string(h3) + "." + to_string(h4) + ":" + to_string(dataPort);
    }

    SOCKET createDataSocket() {
        string passiveInfo = getPassiveMode();
        if (passiveInfo.empty()) return INVALID_SOCKET;

        size_t colonPos = passiveInfo.find(':');
        string dataHost = passiveInfo.substr(0, colonPos);
        int dataPort = stoi(passiveInfo.substr(colonPos + 1));

        SOCKET dataSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket == INVALID_SOCKET) return INVALID_SOCKET;

        struct addrinfo hints, * result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        string portStr = to_string(dataPort);
        if (getaddrinfo(dataHost.c_str(), portStr.c_str(), &hints, &result) != 0) {
            closesocket(dataSocket);
            return INVALID_SOCKET;
        }

        bool connectedFlag = false;
        for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            if (::connect(dataSocket, ptr->ai_addr, (int)ptr->ai_addrlen) != SOCKET_ERROR) {
                connectedFlag = true;
                break;
            }
        }

        freeaddrinfo(result);
        if (!connectedFlag) {
            closesocket(dataSocket);
            return INVALID_SOCKET;
        }
        return dataSocket;
    }

public:
    FTPClient() : controlSocket(INVALID_SOCKET), connected(false), loggedIn(false),
        currentRemoteDir("/"), port(21) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~FTPClient() { disconnect(); WSACleanup(); }

    void resetConnection() {
        if (controlSocket != INVALID_SOCKET) closesocket(controlSocket);
        controlSocket = INVALID_SOCKET;
        connected = false;
        loggedIn = false;
        currentRemoteDir = "/";
    }

    bool connectToServer(const string& serverAddr, int portNum = 21) {
        if (controlSocket != INVALID_SOCKET) closesocket(controlSocket);
        server = serverAddr;
        port = portNum;
        controlSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (controlSocket == INVALID_SOCKET) return false;

        struct addrinfo hints, * result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        string portStr = to_string(port);
        if (getaddrinfo(serverAddr.c_str(), portStr.c_str(), &hints, &result) != 0) {
            closesocket(controlSocket);
            return false;
        }

        bool connectedFlag = false;
        for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            if (::connect(controlSocket, ptr->ai_addr, (int)ptr->ai_addrlen) != SOCKET_ERROR) {
                connectedFlag = true;
                break;
            }
        }

        freeaddrinfo(result);
        if (!connectedFlag) {
            closesocket(controlSocket);
            return false;
        }

        receiveResponse();
        connected = true;
        return true;
    }

    bool login(const string& user, const string& pass) {
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
            if (start != string::npos && end != string::npos)
                currentRemoteDir = response.substr(start + 1, end - start - 1);
            return true;
        }
        return false;
    }

    vector<pair<string, bool>> listFiles() {
        vector<pair<string, bool>> files;
        SOCKET dataSocket = createDataSocket();
        if (dataSocket == INVALID_SOCKET) return files;

        sendCommand("LIST");
        receiveResponse();

        char buffer[65536];
        string data;
        int bytesReceived;
        while ((bytesReceived = recv(dataSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';
            data += buffer;
        }

        closesocket(dataSocket);
        receiveResponse();

        istringstream iss(data);
        string line;
        while (getline(iss, line)) {
            if (line.empty()) continue;
            bool isDir = (line.length() > 0 && line[0] == 'd');
            size_t lastSpace = line.find_last_of(' ');
            if (lastSpace != string::npos) {
                string name = line.substr(lastSpace + 1);
                while (!name.empty() && (name.back() == '\r' || name.back() == '\n')) name.pop_back();
                if (!name.empty() && name != "." && name != "..")
                    files.push_back({ name, isDir });
            }
        }
        return files;
    }

    bool changeDirectory(const string& dir) {
        sendCommand("CWD " + dir);
        string response = receiveResponse();
        if (response.find("250") != string::npos) {
            if (dir == "..") {
                size_t lastSlash = currentRemoteDir.find_last_of('/');
                if (lastSlash > 0) currentRemoteDir = currentRemoteDir.substr(0, lastSlash);
                else currentRemoteDir = "/";
            }
            else if (dir[0] == '/') currentRemoteDir = dir;
            else {
                if (currentRemoteDir != "/") currentRemoteDir += "/" + dir;
                else currentRemoteDir += dir;
            }
            return true;
        }
        return false;
    }

    bool downloadFile(const string& remoteFile, const string& localFile) {
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
        int bytesReceived;
        while ((bytesReceived = recv(dataSocket, buffer, sizeof(buffer), 0)) > 0)
            file.write(buffer, bytesReceived);

        file.close();
        closesocket(dataSocket);
        receiveResponse();
        return true;
    }

    bool uploadFile(const string& localFile, const string& remoteFile) {
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
            int bytesRead = (int)file.gcount();
            if (send(dataSocket, buffer, bytesRead, 0) == SOCKET_ERROR) break;
        }

        file.close();
        closesocket(dataSocket);
        receiveResponse();
        return true;
    }

    bool deleteFile(const string& filename) {
        sendCommand("DELE " + filename);
        return receiveResponse().find("250") != string::npos;
    }

    bool deleteDirectory(const string& dirname) {
        sendCommand("RMD " + dirname);
        return receiveResponse().find("250") != string::npos;
    }

    bool createDirectory(const string& dirname) {
        sendCommand("MKD " + dirname);
        return receiveResponse().find("257") != string::npos;
    }

    bool renameFile(const string& oldName, const string& newName) {
        sendCommand("RNFR " + oldName);
        if (receiveResponse().find("350") == string::npos) return false;
        sendCommand("RNTO " + newName);
        return receiveResponse().find("250") != string::npos;
    }

    void disconnect() {
        if (connected && controlSocket != INVALID_SOCKET) {
            sendCommand("QUIT");
            receiveResponse();
            closesocket(controlSocket);
            controlSocket = INVALID_SOCKET;
            connected = loggedIn = false;
        }
    }

    bool isConnected() { return connected && loggedIn; }
    string getCurrentDir() { return currentRemoteDir; }
    string getServer() { return server; }
};

// ==================== ПОДСВЕТКА СИНТАКСИСА ====================
// ==================== ОПРЕДЕЛЕНИЕ ЯЗЫКА ПО РАСШИРЕНИЮ ====================
string getFileLanguage(const string& filename) {
    size_t dot = filename.find_last_of('.');
    if (dot == string::npos) return "txt";

    string ext = filename.substr(dot + 1);
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp") return "cpp";
    if (ext == "py") return "python";
    if (ext == "js" || ext == "jsx") return "javascript";
    if (ext == "html" || ext == "htm") return "html";
    if (ext == "css") return "css";
    if (ext == "php") return "php";
    return "txt";
}

// ==================== ПОДСВЕТКА СИНТАКСИСА ====================
bool isKeywordCPP(const string& word) {
    static const vector<string> keywords = {
        "int", "void", "char", "float", "double", "bool", "long", "short",
        "unsigned", "signed", "const", "static", "volatile", "extern", "register",
        "if", "else", "switch", "case", "break", "default", "return", "continue",
        "for", "while", "do", "goto", "try", "catch", "throw", "new", "delete",
        "class", "struct", "union", "enum", "typedef", "namespace", "using",
        "public", "private", "protected", "virtual", "friend", "operator",
        "template", "typename", "this", "true", "false", "nullptr", "NULL",
        "include", "define", "ifdef", "ifndef", "endif", "pragma"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isKeywordPython(const string& word) {
    static const vector<string> keywords = {
        "def", "class", "if", "elif", "else", "for", "while", "break", "continue",
        "return", "import", "from", "as", "try", "except", "finally", "raise",
        "with", "as", "lambda", "yield", "global", "nonlocal", "assert", "pass",
        "True", "False", "None", "and", "or", "not", "is", "in", "del", "print"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isKeywordJS(const string& word) {
    static const vector<string> keywords = {
        "function", "var", "let", "const", "if", "else", "switch", "case", "break",
        "default", "return", "for", "while", "do", "continue", "try", "catch",
        "finally", "throw", "new", "delete", "typeof", "instanceof", "class",
        "extends", "super", "import", "export", "default", "async", "await",
        "true", "false", "null", "undefined", "this", "arguments"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isKeywordPHP(const string& word) {
    static const vector<string> keywords = {
        "echo", "print", "if", "else", "elseif", "switch", "case", "break",
        "default", "return", "for", "foreach", "while", "do", "continue",
        "function", "class", "extends", "implements", "public", "private",
        "protected", "static", "final", "abstract", "interface", "trait",
        "namespace", "use", "include", "require", "include_once", "require_once",
        "true", "false", "null", "isset", "empty", "die", "exit", "eval"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isKeywordHTML(const string& word) {
    static const vector<string> keywords = {
        "html", "head", "body", "title", "meta", "link", "script", "style",
        "div", "span", "p", "h1", "h2", "h3", "h4", "h5", "h6", "a", "img",
        "ul", "ol", "li", "table", "tr", "td", "th", "form", "input", "button",
        "select", "option", "textarea", "label", "header", "footer", "nav",
        "section", "article", "aside", "main", "canvas", "svg", "video", "audio"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isKeywordCSS(const string& word) {
    static const vector<string> keywords = {
        "color", "background", "margin", "padding", "border", "width", "height",
        "display", "position", "top", "left", "right", "bottom", "float", "clear",
        "font", "text", "align", "justify", "transform", "transition", "animation",
        "flex", "grid", "inline", "block", "none", "important", "hover", "active",
        "focus", "visited", "first-child", "last-child", "nth-child"
    };
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool isTypeCPP(const string& word) {
    static const vector<string> types = {
        "string", "vector", "map", "list", "set", "pair", "array", "queue",
        "stack", "deque", "shared_ptr", "unique_ptr", "weak_ptr", "function",
        "wstring", "ostream", "istream", "fstream", "ofstream", "ifstream"
    };
    return find(types.begin(), types.end(), word) != types.end();
}

bool isTypePython(const string& word) {
    static const vector<string> types = {
        "list", "dict", "set", "tuple", "str", "int", "float", "bool", "None",
        "range", "enumerate", "zip", "map", "filter", "sorted", "reversed"
    };
    return find(types.begin(), types.end(), word) != types.end();
}

bool isTypeJS(const string& word) {
    static const vector<string> types = {
        "Array", "Object", "String", "Number", "Boolean", "Function", "Promise",
        "Map", "Set", "WeakMap", "WeakSet", "Date", "RegExp", "Error", "Symbol"
    };
    return find(types.begin(), types.end(), word) != types.end();
}

bool isTypePHP(const string& word) {
    static const vector<string> types = {
        "array", "string", "int", "float", "bool", "object", "mixed", "void",
        "callable", "iterable", "resource", "null", "parent", "self", "static"
    };
    return find(types.begin(), types.end(), word) != types.end();
}

// ==================== ОСНОВНАЯ ФУНКЦИЯ ПОДСВЕТКИ ====================
void printWithSyntax(const string& line, const string& language) {
    size_t i = 0;
    int len = line.length();

    while (i < len) {
        // Пробелы
        if (isspace(line[i])) {
            cout << line[i];
            i++;
            continue;
        }

        // Строки в двойных кавычках
        if (line[i] == '"') {
            setColor(COLOR_YELLOW);
            cout << '"';
            i++;
            while (i < len && line[i] != '"') {
                cout << line[i];
                i++;
            }
            if (i < len) cout << '"';
            i++;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // Строки в одинарных кавычках
        if (line[i] == '\'') {
            setColor(COLOR_YELLOW);
            cout << '\'';
            i++;
            while (i < len && line[i] != '\'') {
                cout << line[i];
                i++;
            }
            if (i < len) cout << '\'';
            i++;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // HTML теги
        if (line[i] == '<' && (language == "html" || language == "php")) {
            setColor(COLOR_CYAN);
            cout << '<';
            i++;
            while (i < len && line[i] != '>') {
                cout << line[i];
                i++;
            }
            if (i < len) cout << '>';
            i++;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // CSS селекторы
        if (language == "css" && (line[i] == '.' || line[i] == '#')) {
            setColor(COLOR_CYAN);
            cout << line[i];
            i++;
            while (i < len && (isalnum(line[i]) || line[i] == '-' || line[i] == '_')) {
                cout << line[i];
                i++;
            }
            setColor(COLOR_DEFAULT);
            continue;
        }

        // Комментарии //
        if (i + 1 < len && line[i] == '/' && line[i + 1] == '/') {
            setColor(COLOR_GREEN);
            while (i < len) {
                cout << line[i];
                i++;
            }
            setColor(COLOR_DEFAULT);
            continue;
        }

        // Многострочные комментарии /* */
        if (i + 1 < len && line[i] == '/' && line[i + 1] == '*') {
            setColor(COLOR_GREEN);
            while (i < len) {
                cout << line[i];
                i++;
                if (i > 1 && line[i - 2] == '*' && line[i - 1] == '/') break;
            }
            setColor(COLOR_DEFAULT);
            continue;
        }

        // PHP теги
        if (i + 4 < len && line.substr(i, 5) == "<?php" && language == "php") {
            setColor(COLOR_RED);
            cout << "<?php";
            i += 5;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // CSS свойства
        if (language == "css" && line[i] == ':' && i > 0) {
            setColor(COLOR_RED);
            cout << ':';
            i++;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // Цифры
        if (isdigit(line[i])) {
            setColor(COLOR_CYAN);
            cout << line[i];
            i++;
            setColor(COLOR_DEFAULT);
            continue;
        }

        // Идентификаторы
        if (isalpha(line[i]) || line[i] == '_') {
            string word;
            while (i < len && (isalnum(line[i]) || line[i] == '_')) {
                word += line[i];
                i++;
            }

            bool isKeyword = false;
            bool isType = false;

            if (language == "cpp") {
                isKeyword = isKeywordCPP(word);
                isType = isTypeCPP(word);
            }
            else if (language == "python") {
                isKeyword = isKeywordPython(word);
                isType = isTypePython(word);
            }
            else if (language == "javascript") {
                isKeyword = isKeywordJS(word);
                isType = isTypeJS(word);
            }
            else if (language == "php") {
                isKeyword = isKeywordPHP(word);
                isType = isTypePHP(word);
            }
            else if (language == "html") {
                isKeyword = isKeywordHTML(word);
            }
            else if (language == "css") {
                isKeyword = isKeywordCSS(word);
            }

            if (isKeyword) {
                setColor(COLOR_RED);
                cout << word;
                setColor(COLOR_DEFAULT);
            }
            else if (isType) {
                setColor(COLOR_CYAN);
                cout << word;
                setColor(COLOR_DEFAULT);
            }
            else {
                cout << word;
            }
            continue;
        }

        // Операторы и скобки
        if (line[i] == '<' || line[i] == '>' || line[i] == '=' || line[i] == '+' ||
            line[i] == '-' || line[i] == '*' || line[i] == '/' || line[i] == '%' ||
            line[i] == '!' || line[i] == '&' || line[i] == '|' || line[i] == '^' ||
            line[i] == '(' || line[i] == ')' || line[i] == '{' || line[i] == '}' ||
            line[i] == '[' || line[i] == ']' || line[i] == ';' || line[i] == ',' ||
            line[i] == ':' || line[i] == '?' || line[i] == '.') {
            setColor(COLOR_WHITE);
            cout << line[i];
            setColor(COLOR_DEFAULT);
            i++;
            continue;
        }

        // Всё остальное
        cout << line[i];
        i++;
    }
}

// ==================== РЕДАКТОР ====================
class FileEditor {
private:
    string filename;
    string language;
    vector<string> lines;
    int cursorX, cursorY;
    int viewStart, viewEnd;
    bool modified;

    void loadContent() {
        lines.clear();
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                if (line.length() > 200) line = line.substr(0, 200);
                lines.push_back(line);
            }
            file.close();
        }
        if (lines.empty()) lines.push_back("");
        cursorX = 0;
        cursorY = 0;
        language = getFileLanguage(filename);
    }

    void saveContent() {
        ofstream file(filename);
        if (file.is_open()) {
            for (const auto& line : lines) {
                file << line << "\n";
            }
            file.close();
            modified = false;
        }
    }

    void drawEditor() {
        system("cls");

        // Верхняя рамка
        setColor(COLOR_CYAN);
        cout << "+----------------------------------------------------------------------+\n";
        cout << "| EDITOR: " << filename;
        int padding = 70 - filename.length();
        if (padding < 0) padding = 0;
        cout << string(padding, ' ') << "|\n";
        cout << "| Language: " << language;
        padding = 62 - language.length();
        if (padding < 0) padding = 0;
        cout << string(padding, ' ') << "|\n";
        cout << "+----------------------------------------------------------------------+\n";
        setColor(COLOR_DEFAULT);

        // Определяем видимую область
        int consoleHeight = 22;
        viewStart = max(0, cursorY - consoleHeight / 2);
        viewEnd = min((int)lines.size(), viewStart + consoleHeight);

        for (int i = viewStart; i < viewEnd; i++) {
            // Номер строки
            setColor(COLOR_CYAN);
            printf("%4d ", i + 1);
            setColor(COLOR_DEFAULT);

            // Курсор в начале строки
            if (i == cursorY) {
                setColor(COLOR_YELLOW);
                cout << ">";
                setColor(COLOR_DEFAULT);
            }
            else {
                cout << " ";
            }

            // Содержимое строки с подсветкой
            string line = lines[i];
            if (line.length() > 70) line = line.substr(0, 70);
            printWithSyntax(line, language);

            // Добиваем пробелами
            int lineLen = line.length();
            if (lineLen < 70) cout << string(70 - lineLen, ' ');

            // Показываем позицию курсора
            if (i == cursorY) {
                setColor(COLOR_YELLOW);
                cout << "_";
                setColor(COLOR_DEFAULT);
            }
            else {
                cout << " ";
            }

            cout << "|\n";
        }

        // Заполняем пустые строки
        for (int i = viewEnd; i < viewStart + consoleHeight; i++) {
            cout << "|      " << string(72, ' ') << "|\n";
        }

        // Нижняя рамка
        setColor(COLOR_CYAN);
        cout << "+----------------------------------------------------------------------+\n";
        printf("| Line: %3d / %-3d  Col: %-3d  %-8s",
            cursorY + 1, (int)lines.size(), cursorX + 1, modified ? "MODIFIED" : "SAVED");
        cout << string(40, ' ') << "|\n";
        cout << "+----------------------------------------------------------------------+\n";
        setColor(COLOR_YELLOW);
        cout << "| [Ctrl+S] Save  [Ctrl+X] Exit  [Ins] Line  [Del] Char               |\n";
        cout << "| [Home] Start  [End] End     [Arrows] Move                           |\n";
        setColor(COLOR_DEFAULT);
        cout << "+----------------------------------------------------------------------+\n";
    }

public:
    FileEditor(const string& fname) : filename(fname), cursorX(0), cursorY(0),
        viewStart(0), viewEnd(0), modified(false) {
        loadContent();
    }

    void run() {
        showCursor();

        while (true) {
            drawEditor();

            // Позиционируем курсор
            int cursorScreenY = 4 + (cursorY - viewStart);
            if (cursorScreenY >= 4 && cursorScreenY < 26) {
                COORD coord;
                coord.X = 7 + cursorX;
                coord.Y = cursorScreenY;
                SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
            }

            int key = _getch();

            if (key == 224) {
                key = _getch();
                switch (key) {
                case 72: if (cursorY > 0) cursorY--; break;
                case 80: if (cursorY < (int)lines.size() - 1) cursorY++; break;
                case 75: if (cursorX > 0) cursorX--; break;
                case 77: if (cursorX < (int)lines[cursorY].length()) cursorX++; break;
                case 71: cursorX = 0; break;
                case 79: cursorX = lines[cursorY].length(); break;
                case 83:
                    if (cursorX < (int)lines[cursorY].length()) {
                        lines[cursorY].erase(cursorX, 1);
                        modified = true;
                    }
                    else if (cursorY < (int)lines.size() - 1) {
                        lines[cursorY] += lines[cursorY + 1];
                        lines.erase(lines.begin() + cursorY + 1);
                        modified = true;
                    }
                    break;
                case 82:
                    lines.insert(lines.begin() + cursorY + 1, "");
                    modified = true;
                    break;
                }
                if (cursorX > (int)lines[cursorY].length()) cursorX = lines[cursorY].length();
            }
            else if (key == 13) {
                string newLine = lines[cursorY].substr(cursorX);
                lines[cursorY] = lines[cursorY].substr(0, cursorX);
                lines.insert(lines.begin() + cursorY + 1, newLine);
                cursorY++;
                cursorX = 0;
                modified = true;
            }
            else if (key == 8) {
                if (cursorX > 0) {
                    cursorX--;
                    lines[cursorY].erase(cursorX, 1);
                    modified = true;
                }
                else if (cursorY > 0) {
                    cursorX = lines[cursorY - 1].length();
                    lines[cursorY - 1] += lines[cursorY];
                    lines.erase(lines.begin() + cursorY);
                    cursorY--;
                    modified = true;
                }
            }
            else if (key == 19) {
                saveContent();
            }
            else if (key == 24) {
                if (modified) {
                    setColor(COLOR_YELLOW);
                    cout << "\n| Save changes? (y/n): ";
                    setColor(COLOR_DEFAULT);
                    if (_getch() == 'y') saveContent();
                }
                break;
            }
            else if (key >= 32 && key <= 126) {
                if (cursorX < (int)lines[cursorY].length()) {
                    lines[cursorY].insert(cursorX, 1, (char)key);
                }
                else {
                    lines[cursorY] += (char)key;
                }
                cursorX++;
                if (lines[cursorY].length() > 200) lines[cursorY] = lines[cursorY].substr(0, 200);
                modified = true;
            }
        }

        hideCursor();
    }

    bool isModified() const { return modified; }
};

// ==================== ДЕРЕВО ФАЙЛОВ ====================
void printTree(FTPClient& ftp, const string& path, int level) {
    string indent = string(level * 2, ' ');
    string oldDir = ftp.getCurrentDir();

    if (ftp.changeDirectory(path)) {
        auto files = ftp.listFiles();
        for (size_t i = 0; i < files.size(); i++) {
            bool isLast = (i == files.size() - 1);
            string prefix = indent;
            if (level > 0) {
                prefix += isLast ? "└── " : "├── ";
            }

            if (files[i].second) {
                setColor(COLOR_CYAN);
                cout << prefix << "[DIR] " << files[i].first << "/\n";
                setColor(COLOR_DEFAULT);
                printTree(ftp, files[i].first, level + 1);
            }
            else {
                setColor(COLOR_GREEN);
                cout << prefix << "[FILE] " << files[i].first << "\n";
                setColor(COLOR_DEFAULT);
            }
        }
        ftp.changeDirectory(oldDir);
    }
}

void showTreeView(FTPClient& ftp, const vector<pair<string, bool>>& currentFiles, const string& currentPath) {
    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         " << _("tree") << "                                         |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);
    cout << "| " << _("folder") << ": " << currentPath << "\n";
    cout << "+============================================================================+\n\n";

    string oldDir = ftp.getCurrentDir();

    for (size_t i = 0; i < currentFiles.size(); i++) {
        bool isLast = (i == currentFiles.size() - 1);
        string prefix = isLast ? "└── " : "├── ";

        if (currentFiles[i].second) {
            setColor(COLOR_CYAN);
            cout << prefix << "[DIR] " << currentFiles[i].first << "/\n";
            setColor(COLOR_DEFAULT);
            printTree(ftp, currentFiles[i].first, 1);
        }
        else {
            setColor(COLOR_GREEN);
            cout << prefix << "[FILE] " << currentFiles[i].first << "\n";
            setColor(COLOR_DEFAULT);
        }
    }

    ftp.changeDirectory(oldDir);

    cout << "\n+============================================================================+\n";
    cout << "| [Any key] " << _("back") << "\n";
    cout << "+============================================================================+\n";
    _getch();
}

// ==================== НАСТРОЙКИ МЕНЮ ====================
void showSettingsMenu() {
    vector<string> langs = { "ru", "en" };
    int langIdx = 0;

    for (size_t i = 0; i < langs.size(); i++) if (langs[i] == currentLanguage) langIdx = i;

    while (true) {
        clearScreen();
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         " << _("settings") << "                                           |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);

        cout << "|                                                                            |\n";
        cout << "| 1. " << _("language") << ": " << langs[langIdx] << "\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";
        cout << "| [1] " << _("change_language") << "  [Enter] " << _("save") << "  [Esc] " << _("cancel") << "\n";
        cout << "+============================================================================+\n";

        int key = _getch();
        if (key == 27) break;
        if (key == 13) {
            loadLanguage(langs[langIdx]);
            saveAllSettings();
            setColor(COLOR_GREEN);
            cout << "\n| " << _("settings_saved") << "\n";
            setColor(COLOR_DEFAULT);
            Sleep(1000);
            break;
        }
        if (key == '1') langIdx = (langIdx + 1) % langs.size();
    }
}

// ==================== ГЛАВНЫЙ ИНТЕРФЕЙС ====================
FTPClient ftp;
int selectedIndex = 0;
vector<pair<string, bool>> currentFiles;
string message;
int messageTimer = 0;
bool running = true;

void showMessage(const string& msg, bool isError = false) {
    message = msg;
    messageTimer = 30;
    addToHistory(msg);
}

void refreshFileList() {
    if (ftp.isConnected()) {
        currentFiles = ftp.listFiles();
        if (selectedIndex >= (int)currentFiles.size())
            selectedIndex = max(0, (int)currentFiles.size() - 1);
    }
}

void addNewProfile() {
    FTPProfile newProfile;
    int currentField = 0;
    bool showPassword = false;
    newProfile.port = "21";

    while (true) {
        system("cls");

        setColor(COLOR_CYAN);
        cout << "+------------------------------------------------------------+\n";
        cout << "|                 " << _("add_profile") << "                          |\n";
        cout << "+------------------------------------------------------------+\n";
        setColor(COLOR_DEFAULT);

        cout << "|                                                            |\n";
        cout << "|  [Up/Down] - " << _("select_field") << "    [Enter] - " << _("next") << "                  |\n";
        cout << "|  [Tab] - " << _("show_password") << "    [Esc] - " << _("cancel") << "              |\n";
        cout << "|  [Ctrl+S] - " << _("save_profile") << "                              |\n";
        cout << "|                                                            |\n";
        cout << "+------------------------------------------------------------+\n";
        cout << "|                                                            |\n";

        string fields[] = { "Name:", "Server:", "Port:", "Username:", "Password:" };
        string* values[] = { &newProfile.name, &newProfile.server, &newProfile.port,
                            &newProfile.username, &newProfile.password };

        for (int i = 0; i < 5; i++) {
            cout << "| ";

            // Индикатор активного поля
            if (i == currentField) {
                setColor(COLOR_YELLOW);
                cout << ">>";
                setColor(COLOR_DEFAULT);
            }
            else {
                cout << "  ";
            }

            cout << "  " << fields[i] << " ";

            // Значение
            string displayValue;
            if (i == 4 && !showPassword && !newProfile.password.empty())
                displayValue = string(newProfile.password.length(), '*');
            else
                displayValue = *values[i];

            if (displayValue.empty()) {
                setColor(COLOR_RED);
                cout << "<empty>";
                setColor(COLOR_DEFAULT);
            }
            else {
                if (i == currentField) setColor(COLOR_YELLOW);
                else setColor(COLOR_DEFAULT);
                cout << displayValue;
                setColor(COLOR_DEFAULT);
            }

            // Показываем курсор в активном поле
            if (i == currentField) {
                setColor(COLOR_YELLOW);
                cout << "_";
                setColor(COLOR_DEFAULT);
            }

            // Добиваем пробелами
            int len = 4 + fields[i].length() + displayValue.length() + (i == currentField ? 1 : 0);
            if (i == 2 && newProfile.port == "21") {
                cout << " (default)";
                len += 10;
            }
            cout << string(60 - len, ' ') << " |\n";
        }

        cout << "|                                                            |\n";
        cout << "+------------------------------------------------------------+\n";

        if (!message.empty()) {
            setColor(message.find("error") != string::npos ? COLOR_RED : COLOR_GREEN);
            cout << "| " << message << string(58 - message.length(), ' ') << " |\n";
            cout << "+------------------------------------------------------------+\n";
            setColor(COLOR_DEFAULT);
            if (--messageTimer <= 0) message.clear();
        }

        // Показываем курсор в активном поле
        int cursorX = 8 + fields[currentField].length() + values[currentField]->length();
        if (currentField == 4 && !showPassword && !newProfile.password.empty()) {
            cursorX = 8 + fields[currentField].length() + newProfile.password.length();
        }
        showCursorAt(cursorX, 10 + currentField);

        int key = _getch();

        // Стрелки
        if (key == 224) {
            key = _getch();
            if (key == 72 && currentField > 0) currentField--;
            if (key == 80 && currentField < 4) currentField++;
            continue;
        }

        if (key == 27) { hideCursor(); return; }

        if (key == 9 && currentField == 4) {
            showPassword = !showPassword;
            continue;
        }

        if (key == 19) { // Ctrl+S
            if (newProfile.name.empty()) {
                showMessage("Enter profile name!", true);
                continue;
            }
            if (newProfile.server.empty()) {
                showMessage("Enter server!", true);
                continue;
            }
            if (newProfile.username.empty()) {
                showMessage("Enter username!", true);
                continue;
            }
            if (newProfile.password.empty()) {
                showMessage("Enter password!", true);
                continue;
            }
            if (newProfile.port.empty()) newProfile.port = "21";

            profiles.push_back(newProfile);
            saveProfiles();
            showMessage(_("profile_added"));
            hideCursor();
            return;
        }

        if (key == 13) {  // Enter
            if (currentField < 4) {
                currentField++;
            }
            else {
                if (newProfile.name.empty()) {
                    showMessage("Enter profile name!", true);
                    continue;
                }
                if (newProfile.server.empty()) {
                    showMessage("Enter server!", true);
                    continue;
                }
                if (newProfile.username.empty()) {
                    showMessage("Enter username!", true);
                    continue;
                }
                if (newProfile.password.empty()) {
                    showMessage("Enter password!", true);
                    continue;
                }
                if (newProfile.port.empty()) newProfile.port = "21";

                profiles.push_back(newProfile);
                saveProfiles();
                showMessage(_("profile_added"));
                hideCursor();
                return;
            }
            continue;
        }

        if (key == 8 && !values[currentField]->empty()) {  // Backspace
            values[currentField]->pop_back();
            continue;
        }

        if (key >= 32 && key <= 126) {
            char ch = (char)key;
            if (currentField == 2) { // Порт - только цифры
                if (isdigit(ch) && values[currentField]->length() < 5) {
                    if (*values[currentField] == "21") *values[currentField] = "";
                    *values[currentField] += ch;
                }
            }
            else {
                if (values[currentField]->length() < 50) {
                    *values[currentField] += ch;
                }
            }
            continue;
        }
    }
}

void connectWithProfile(const FTPProfile& profile) {
    clearScreen();
    cout << "+============================================================================+\n";
    cout << "|                         " << _("connecting") << "                                     |\n";
    cout << "+============================================================================+\n";
    cout << "| " << _("profile") << ": " << profile.name << "\n";
    cout << "| " << _("server") << ": " << profile.server << "\n";
    cout << "| " << _("port") << ": " << profile.port << "\n";
    cout << "| " << _("username") << ": " << profile.username << "\n";
    cout << "| " << _("connecting") << "...\n";
    cout << "+============================================================================+\n";

    if (ftp.connectToServer(profile.server, stoi(profile.port))) {
        cout << "| " << _("auth") << "...\n";
        if (ftp.login(profile.username, profile.password)) {
            setColor(COLOR_GREEN);
            cout << "| " << _("connected_success") << "\n";
            setColor(COLOR_DEFAULT);
            showMessage(_("connected_to") + " " + profile.server);
            refreshFileList();
            selectedIndex = 0;
            this_thread::sleep_for(chrono::milliseconds(1000));
            return;
        }
    }
    setColor(COLOR_RED);
    cout << "| " << _("connect_error") << "\n";
    setColor(COLOR_DEFAULT);
    showMessage(_("connect_error"), true);
    ftp.resetConnection();
    this_thread::sleep_for(chrono::milliseconds(1500));
}

void selectProfile() {
    if (profiles.empty()) {
        showMessage(_("no_profiles"), true);
        addNewProfile();
        return;
    }

    int currentSelection = 0;
    bool addingNew = false;

    while (true) {
        clearScreen();
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         " << _("select_profile") << "                                 |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);

        for (int i = 0; i < (int)profiles.size(); i++) {
            setColor((i == currentSelection && !addingNew) ? COLOR_YELLOW : COLOR_DEFAULT);
            cout << "| " << ((i == currentSelection && !addingNew) ? ">" : " ") << " [" << (i + 1) << "] "
                << profiles[i].name << " (" << profiles[i].server << ")\n";
        }

        setColor(addingNew ? COLOR_YELLOW : COLOR_DEFAULT);
        cout << "| " << (addingNew ? ">" : " ") << " [+] " << _("add_new") << "\n";
        setColor(COLOR_DEFAULT);

        cout << "+============================================================================+\n";
        cout << "|  [↑] [↓] " << _("select") << "    [Enter] " << _("choose") << "    [N] " << _("new") << "    [D] " << _("delete") << "    [Esc] " << _("exit") << " |\n";
        cout << "+============================================================================+\n";

        int key = _getch();
        if (key == 224) {
            key = _getch();
            if (key == 72) {
                if (currentSelection > 0) currentSelection--;
                else if (currentSelection == 0 && addingNew) addingNew = false;
                else if (currentSelection == 0 && !addingNew) { currentSelection = profiles.size() - 1; addingNew = false; }
            }
            if (key == 80) {
                if (!addingNew && currentSelection < (int)profiles.size() - 1) currentSelection++;
                else if (!addingNew && currentSelection == (int)profiles.size() - 1) addingNew = true;
                else if (addingNew) { addingNew = false; currentSelection = 0; }
            }
            continue;
        }
        if (key == 27) return;
        if (key == 'n' || key == 'N') { addNewProfile(); return; }
        if (key == 'd' || key == 'D') {
            if (!addingNew && currentSelection < (int)profiles.size()) {
                cout << "\n| " << _("delete_profile") << "? (y/n): ";
                if (_getch() == 'y') {
                    profiles.erase(profiles.begin() + currentSelection);
                    saveProfiles();
                    showMessage(_("profile_deleted"));
                    if (profiles.empty()) return;
                    if (currentSelection >= (int)profiles.size())
                        currentSelection = profiles.size() - 1;
                }
            }
            continue;
        }
        if (key == 13) {
            if (addingNew) addNewProfile();
            else connectWithProfile(profiles[currentSelection]);
            return;
        }
    }
}

void editFile() {
    if (!ftp.isConnected()) { showMessage(_("connect_first"), true); return; }
    if (currentFiles.empty() || currentFiles[selectedIndex].second) {
        showMessage(_("select_file"), true);
        return;
    }

    string filename = currentFiles[selectedIndex].first;
    string tempFile = "temp_" + filename;

    showMessage(_("downloading") + " " + filename);
    if (ftp.downloadFile(filename, tempFile)) {
        FileEditor editor(tempFile);
        editor.run();
        if (editor.isModified()) {
            showMessage(_("uploading") + " " + filename);
            if (ftp.uploadFile(tempFile, filename)) {
                showMessage(_("file_updated"));
                refreshFileList();
            }
            else {
                showMessage(_("upload_error"), true);
            }
        }
        fs::remove(tempFile);
    }
    else {
        showMessage(_("download_error"), true);
    }
}

void downloadItem() {
    if (!ftp.isConnected()) { showMessage(_("connect_first"), true); return; }
    if (currentFiles.empty()) { showMessage(_("no_items"), true); return; }

    string name = currentFiles[selectedIndex].first;
    bool isDir = currentFiles[selectedIndex].second;

    if (isDir) {
        clearScreen();
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         " << _("download_folder") << "                                 |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "| " << _("folder") << ": " << name << "\n";
        cout << "| " << _("local_path") << " (Enter - " << name << "): ";

        showCursor();
        string localPath;
        getline(cin, localPath);
        if (localPath.empty()) localPath = name;

        cout << "| " << _("downloading") << "...\n";
        cout << "+============================================================================+\n\n";

        // Упрощённое скачивание папки
        string oldDir = ftp.getCurrentDir();
        if (ftp.changeDirectory(name)) {
            fs::create_directories(localPath);
            auto files = ftp.listFiles();
            for (const auto& file : files) {
                if (!file.second) {
                    cout << "| " << _("downloading") << " " << file.first << "... ";
                    if (ftp.downloadFile(file.first, localPath + "/" + file.first)) {
                        setColor(COLOR_GREEN);
                        cout << "OK\n";
                        setColor(COLOR_DEFAULT);
                    }
                    else {
                        setColor(COLOR_RED);
                        cout << "FAIL\n";
                        setColor(COLOR_DEFAULT);
                    }
                }
            }
            ftp.changeDirectory(oldDir);
            setColor(COLOR_GREEN);
            cout << "\n| " << _("folder_downloaded") << "\n";
            showMessage(_("folder_downloaded"));
        }
        else {
            setColor(COLOR_RED);
            cout << "\n| " << _("download_error") << "\n";
            showMessage(_("download_error"), true);
        }
        hideCursor();
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
    else {
        showMessage(_("downloading") + " " + name);
        if (ftp.downloadFile(name, name)) {
            showMessage(_("downloaded") + " " + name);
        }
        else {
            showMessage(_("download_error"), true);
        }
    }
}

void uploadItem() {
    if (!ftp.isConnected()) { showMessage(_("connect_first"), true); return; }

    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         " << _("select_upload_type") << "                           |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);
    cout << "|                                                                            |\n";
    cout << "|   [F] " << _("upload_file") << "\n";
    cout << "|   [D] " << _("upload_folder") << "\n";
    cout << "|                                                                            |\n";
    cout << "|   [Esc] " << _("cancel") << "\n";
    cout << "|                                                                            |\n";
    cout << "+============================================================================+\n";

    int choice = _getch();

    if (choice == 'f' || choice == 'F') {
        clearScreen();
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         " << _("upload_file") << "                                     |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "| " << _("local_file") << ": ";
        showCursor();
        string localFile;
        cin >> localFile;
        string remoteFile = localFile.substr(localFile.find_last_of("/\\") + 1);
        cout << "| " << _("uploading") << "...\n";
        cout << "+============================================================================+\n";

        if (ftp.uploadFile(localFile, remoteFile)) {
            setColor(COLOR_GREEN);
            cout << "| " << _("uploaded") << "\n";
            showMessage(_("uploaded"));
            refreshFileList();
        }
        else {
            setColor(COLOR_RED);
            cout << "| " << _("upload_error") << "\n";
            showMessage(_("upload_error"), true);
        }
        hideCursor();
        this_thread::sleep_for(chrono::milliseconds(1500));
    }
    else if (choice == 'd' || choice == 'D') {
        if (currentFiles.empty() || !currentFiles[selectedIndex].second) {
            showMessage(_("select_remote_folder"), true);
            hideCursor();
            this_thread::sleep_for(chrono::milliseconds(1500));
            return;
        }

        string remoteFolder = currentFiles[selectedIndex].first;

        clearScreen();
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         " << _("upload_folder") << "                                 |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "| " << _("remote_folder") << ": " << remoteFolder << "\n";
        cout << "|                                                                            |\n";
        cout << "| " << _("local_folder") << ": ";

        showCursor();
        string localFolder;
        getline(cin, localFolder);

        if (!fs::exists(localFolder) || !fs::is_directory(localFolder)) {
            setColor(COLOR_RED);
            cout << "| " << _("folder_not_found") << "\n";
            setColor(COLOR_DEFAULT);
            showMessage(_("folder_not_found"), true);
            hideCursor();
            this_thread::sleep_for(chrono::milliseconds(2000));
            return;
        }

        cout << "| " << _("uploading") << "...\n";
        cout << "+============================================================================+\n";

        // Упрощённая загрузка папки
        string oldDir = ftp.getCurrentDir();
        if (ftp.changeDirectory(remoteFolder)) {
            for (const auto& entry : fs::directory_iterator(localFolder)) {
                string name = entry.path().filename().string();
                if (!fs::is_directory(entry.path())) {
                    cout << "| " << _("uploading") << " " << name << "... ";
                    if (ftp.uploadFile(entry.path().string(), name)) {
                        setColor(COLOR_GREEN);
                        cout << "OK\n";
                        setColor(COLOR_DEFAULT);
                    }
                    else {
                        setColor(COLOR_RED);
                        cout << "FAIL\n";
                        setColor(COLOR_DEFAULT);
                    }
                }
            }
            ftp.changeDirectory(oldDir);
            setColor(COLOR_GREEN);
            cout << "\n| " << _("folder_uploaded") << "\n";
            showMessage(_("folder_uploaded"));
            refreshFileList();
        }
        else {
            setColor(COLOR_RED);
            cout << "\n| " << _("upload_error") << "\n";
            showMessage(_("upload_error"), true);
        }
        hideCursor();
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

void drawExitScreen() {
    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                              " << _("exit") << "                            |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);
    cout << "|                                                                            |\n";
    cout << "|                        " << _("confirm_exit") << "                       |\n";
    cout << "|                                                                            |\n";
    cout << "+============================================================================+\n";
    cout << "|                                                                            |\n";
    setColor(COLOR_YELLOW);
    cout << "|                          [Y] " << _("yes") << "            [N] " << _("no") << "                         |\n";
    setColor(COLOR_DEFAULT);
    cout << "|                                                                            |\n";
    cout << "+============================================================================+\n";

    while (true) {
        int key = _getch();
        if (key == 'y' || key == 'Y') { running = false; break; }
        if (key == 'n' || key == 'N' || key == 27) return;
    }
}

void handleRename() {
    if (!ftp.isConnected()) { showMessage(_("connect_first"), true); return; }
    if (currentFiles.empty()) { showMessage(_("no_items"), true); return; }

    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         " << _("rename") << "                                         |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);

    string oldName = currentFiles[selectedIndex].first;
    cout << "| " << _("old_name") << ": " << oldName << "\n";
    cout << "| " << _("new_name") << ": ";
    showCursor();
    string newName;
    cin >> newName;

    if (!newName.empty() && ftp.renameFile(oldName, newName)) {
        showMessage(_("renamed_to") + " " + newName);
        refreshFileList();
    }
    else if (!newName.empty()) showMessage(_("rename_error"), true);

    hideCursor();
    this_thread::sleep_for(chrono::milliseconds(1500));
}

void handleFileInfo() {
    if (!ftp.isConnected()) { showMessage(_("connect_first"), true); return; }
    if (currentFiles.empty()) { showMessage(_("no_items"), true); return; }

    string name = currentFiles[selectedIndex].first;
    bool isDir = currentFiles[selectedIndex].second;

    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         " << _("info") << "                                           |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);
    cout << "| " << _("name") << ": " << name << "\n";
    cout << "| " << _("type") << ": " << (isDir ? _("folder") : _("file")) << "\n";
    cout << "+============================================================================+\n";
    cout << "| [Any key] " << _("back") << "\n";
    cout << "+============================================================================+\n";
    _getch();
}

void drawMainInterface() {
    clearScreen();
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         F T P   C L I E N T   v2.0                         |\n";
    cout << "|                                                                             |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);

    if (ftp.isConnected()) {
        setColor(COLOR_GREEN);
        cout << "| " << _("connected") << "\n";
        setColor(COLOR_DEFAULT);
        cout << "| " << _("server") << ": " << ftp.getServer() << "\n";
        cout << "| " << _("folder") << ":  " << ftp.getCurrentDir() << "\n";
        cout << "+============================================================================+\n";
        cout << "| " << _("files_folders") << " (" << currentFiles.size() << "):\n";
        cout << "+============================================================================+\n";

        if (currentFiles.empty()) cout << "|                               " << _("empty") << "                                    |\n";
        else for (int i = 0; i < (int)currentFiles.size() && i < 15; i++) {
            setColor(i == selectedIndex ? COLOR_YELLOW : COLOR_DEFAULT);
            cout << "| " << (i == selectedIndex ? ">" : " ") << " ";
            setColor(currentFiles[i].second ? COLOR_CYAN : COLOR_GREEN);
            cout << (currentFiles[i].second ? "[DIR] " : "[FILE]");
            setColor(COLOR_DEFAULT);
            string name = currentFiles[i].first;
            if (name.length() > 38) name = name.substr(0, 35) + "...";
            cout << left << setw(38) << name;
            if (i == selectedIndex) { setColor(COLOR_YELLOW); cout << " <"; }
            cout << " |\n";
        }

        cout << "+============================================================================+\n";
        cout << "|                                                                            |\n";
        cout << "| " << _("commands") << " (" << _("arrows") << " [↑] [↓]):\n";
        cout << "|                                                                            |\n";
        cout << "|   [Enter] " << _("open") << "    [Backspace] " << _("back") << "\n";
        cout << "|   [D] " << _("download") << "    [U] " << _("upload") << "    [E] " << _("edit") << "\n";
        cout << "|   [T] " << _("delete") << "      [C] " << _("create_folder") << "      [N] " << _("rename") << "\n";
        cout << "|   [I] " << _("info") << "       [V] " << _("tree") << "       [R] " << _("refresh") << "\n";
        cout << "|   [O] " << _("settings") << "    [F] " << _("disconnect") << "   [H] " << _("history") << "\n";
        cout << "|   [Q] " << _("exit") << "\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";
    }
    else {
        cout << "| [" << _("not_connected") << "]                                                              |\n";
        cout << "+============================================================================+\n";
        cout << "|                                                                            |\n";
        cout << "| " << _("commands") << ":\n";
        cout << "|                                                                            |\n";
        cout << "|   [Enter] " << _("select_profile") << "    [O] " << _("settings") << "    [H] " << _("history") << "    [Q] " << _("exit") << "\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";
    }

    if (!message.empty()) {
        setColor(message.find(_("error")) != string::npos ? COLOR_RED : COLOR_GREEN);
        cout << "| " << message << string(76 - message.length(), ' ') << " |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        if (--messageTimer <= 0) message.clear();
    }
}

// ==================== MAIN ====================
int main() {
    setRussianConsole();
    hideCursor();

    loadAllSettings();
    loadProfiles();

    while (running) {
        drawMainInterface();

        int key = _getch();

        if (!ftp.isConnected()) {
            switch (key) {
            case 13: selectProfile(); break;
            case 'o': case 'O': showSettingsMenu(); break;
            case 'h': case 'H': showHistory(); break;
            case 'q': case 'Q': drawExitScreen(); break;
            }
        }
        else {
            switch (key) {
            case 224:
                key = _getch();
                if (key == 72 && selectedIndex > 0) selectedIndex--;
                if (key == 80 && selectedIndex < (int)currentFiles.size() - 1) selectedIndex++;
                break;
            case 13:
                if (!currentFiles.empty() && currentFiles[selectedIndex].second) {
                    string folderName = currentFiles[selectedIndex].first;
                    if (ftp.changeDirectory(folderName)) {
                        refreshFileList();
                        selectedIndex = 0;
                        showMessage(_("opened") + " " + folderName);
                    }
                    else showMessage(_("cannot_open"), true);
                }
                break;
            case 8:
                if (ftp.changeDirectory("..")) {
                    refreshFileList();
                    selectedIndex = 0;
                    showMessage(_("back"));
                }
                break;
            case 'd': case 'D': downloadItem(); break;
            case 'u': case 'U': uploadItem(); break;
            case 'e': case 'E': editFile(); break;
            case 't': case 'T':
                if (!currentFiles.empty()) {
                    string name = currentFiles[selectedIndex].first;
                    bool isDir = currentFiles[selectedIndex].second;
                    cout << "\n| " << _("confirm_delete") << "? (y/n): ";
                    if (_getch() == 'y') {
                        if ((isDir && ftp.deleteDirectory(name)) || (!isDir && ftp.deleteFile(name))) {
                            showMessage(_("deleted") + " " + name);
                            refreshFileList();
                        }
                        else showMessage(_("delete_error"), true);
                    }
                }
                break;
            case 'c': case 'C':
            {
                clearScreen();
                setColor(COLOR_CYAN);
                cout << "+============================================================================+\n";
                cout << "|                         " << _("create_folder") << "                                   |\n";
                cout << "+============================================================================+\n";
                setColor(COLOR_DEFAULT);
                cout << "| " << _("folder_name") << " (Enter - " << _("cancel") << "): ";
                showCursor();
                string dirname;
                getline(cin, dirname);
                if (!dirname.empty() && ftp.createDirectory(dirname)) {
                    setColor(COLOR_GREEN);
                    cout << "| " << _("folder_created") << "\n";
                    showMessage(_("folder_created"));
                    refreshFileList();
                }
                else if (!dirname.empty()) {
                    setColor(COLOR_RED);
                    cout << "| " << _("create_error") << "\n";
                    showMessage(_("create_error"), true);
                }
                hideCursor();
                this_thread::sleep_for(chrono::milliseconds(1500));
            }
            break;
            case 'n': case 'N': handleRename(); break;
            case 'i': case 'I': handleFileInfo(); break;
            case 'v': case 'V': showTreeView(ftp, currentFiles, ftp.getCurrentDir()); break;
            case 'r': case 'R': refreshFileList(); showMessage(_("refreshed")); break;
            case 'o': case 'O': showSettingsMenu(); break;
            case 'h': case 'H': showHistory(); break;
            case 'f': case 'F':
                ftp.disconnect();
                currentFiles.clear();
                selectedIndex = 0;
                showMessage(_("disconnected"));
                break;
            case 'q': case 'Q': drawExitScreen(); break;
            }
        }
    }

    clearScreen();
    setColor(COLOR_CYAN);
    cout << "\n\n\t\t+=====================================+\n";
    cout << "\t\t|         " << _("goodbye") << "!                |\n";
    cout << "\t\t+=====================================+\n\n";
    setColor(COLOR_DEFAULT);
    this_thread::sleep_for(chrono::milliseconds(1000));

    return 0;
}