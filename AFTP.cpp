// ftp_client_final.cpp
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
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

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

// Цвета консоли
void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

#define COLOR_DEFAULT 7
#define COLOR_GREEN 10
#define COLOR_RED 12
#define COLOR_YELLOW 14
#define COLOR_CYAN 11
#define COLOR_WHITE 15

// Структура профиля
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

void setRussianConsole() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    setlocale(LC_ALL, "Russian");
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

class ProfileManager {
private:
    vector<FTPProfile> profiles;
    string profilesFile = "ftp_profiles.dat";

public:
    void load() {
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

    void save() {
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

    void add(const FTPProfile& profile) { profiles.push_back(profile); save(); }
    void remove(int index) { if (index >= 0 && index < (int)profiles.size()) { profiles.erase(profiles.begin() + index); save(); } }
    const vector<FTPProfile>& getAll() const { return profiles; }
    bool isEmpty() const { return profiles.empty(); }
    int size() const { return profiles.size(); }
    const FTPProfile& get(int index) const { return profiles[index]; }
};

class HistoryManager {
private:
    vector<string> history;
    string historyFile = "ftp_history.txt";

public:
    void add(const string& action) {
        time_t now = time(0);
        char* dt = ctime(&now);
        string timestamp(dt);
        timestamp.pop_back();
        history.push_back("[" + timestamp + "] " + action);

        ofstream file(historyFile, ios::app);
        if (file.is_open()) {
            file << "[" << timestamp << "] " << action << endl;
            file.close();
        }
        if (history.size() > 100) history.erase(history.begin());
    }

    void show() {
        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                           ИСТОРИЯ ДЕЙСТВИЙ                                 |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);

        if (history.empty()) cout << "| История пуста                                                           |\n";
        else for (const auto& entry : history) {
            cout << "| " << entry;
            if (entry.length() < 76) cout << string(76 - entry.length(), ' ');
            cout << " |\n";
        }

        cout << "+============================================================================+\n";
        cout << "| [ЛЮБАЯ КЛАВИША] Назад                                                      |\n";
        cout << "+============================================================================+\n";
        _getch();
    }
};

class FTPClient {
private:
    SOCKET controlSocket;
    string server, username, password, currentRemoteDir;
    int port;
    bool connected, loggedIn;

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
        server = "";
        username = "";
        password = "";
        port = 21;
        currentRemoteDir = "/";
    }

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
            controlSocket = INVALID_SOCKET;
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
            controlSocket = INVALID_SOCKET;
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

    vector<pair<string, bool>> listFiles(const string& path = "") {
        vector<pair<string, bool>> files;
        string oldDir = currentRemoteDir;

        if (!path.empty() && !changeDirectory(path)) return files;

        SOCKET dataSocket = createDataSocket();
        if (dataSocket == INVALID_SOCKET) {
            if (!path.empty()) changeDirectory(oldDir);
            return files;
        }

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

        if (!path.empty()) changeDirectory(oldDir);
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

    bool downloadFolder(const string& remoteFolder, const string& localFolder, int depth = 0) {
        string indent = string(depth * 2, ' ');

        if (!fs::exists(localFolder)) {
            fs::create_directories(localFolder);
            cout << indent << "[СОЗДАНА] " << localFolder << "\n";
        }

        string oldDir = currentRemoteDir;
        if (!changeDirectory(remoteFolder)) {
            cout << indent << "[ОШИБКА] Не удалось перейти в " << remoteFolder << "\n";
            changeDirectory(oldDir);
            return false;
        }

        auto files = listFiles();
        int fileCount = 0, folderCount = 0;

        for (const auto& item : files) {
            if (item.second) {
                folderCount++;
                cout << indent << "[ПАПКА] " << item.first << "\n";
                string newRemotePath = item.first;
                string newLocalPath = localFolder + "/" + item.first;
                downloadFolder(newRemotePath, newLocalPath, depth + 1);
            }
            else {
                fileCount++;
                cout << indent << "[ФАЙЛ]  " << item.first << " ... ";
                string localFile = localFolder + "/" + item.first;
                if (downloadFile(item.first, localFile)) {
                    setColor(COLOR_GREEN);
                    cout << "OK\n";
                    setColor(COLOR_DEFAULT);
                }
                else {
                    setColor(COLOR_RED);
                    cout << "ОШИБКА\n";
                    setColor(COLOR_DEFAULT);
                }
            }
        }

        cout << indent << "[ИТОГО] Папок: " << folderCount << ", Файлов: " << fileCount << "\n";
        changeDirectory(oldDir);
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

    bool uploadFolder(const string& localFolder, const string& remoteFolder, int depth = 0) {
        string indent = string(depth * 2, ' ');

        if (!createDirectory(remoteFolder)) {
            cout << indent << "[СУЩЕСТВУЕТ] " << remoteFolder << "\n";
        }
        else {
            cout << indent << "[СОЗДАНА] " << remoteFolder << "\n";
        }

        string oldDir = currentRemoteDir;

        if (!changeDirectory(remoteFolder)) {
            cout << indent << "[ОШИБКА] Не удалось перейти в " << remoteFolder << "\n";
            changeDirectory(oldDir);
            return false;
        }

        vector<pair<string, bool>> localItems;
        try {
            for (const auto& entry : fs::directory_iterator(localFolder)) {
                string name = entry.path().filename().string();
                bool isDir = fs::is_directory(entry.path());
                localItems.push_back({ name, isDir });
            }
        }
        catch (const exception& e) {
            cout << indent << "[ОШИБКА] Не удалось прочитать локальную папку\n";
            changeDirectory(oldDir);
            return false;
        }

        int fileCount = 0, folderCount = 0;

        for (const auto& item : localItems) {
            if (item.second) {
                folderCount++;
                cout << indent << "[ПАПКА] " << item.first << "\n";
                string newLocalPath = localFolder + "/" + item.first;
                string newRemotePath = item.first;
                uploadFolder(newLocalPath, newRemotePath, depth + 1);
            }
            else {
                fileCount++;
                cout << indent << "[ФАЙЛ]  " << item.first << " ... ";
                string localFile = localFolder + "/" + item.first;
                string remoteFile = item.first;

                if (uploadFile(localFile, remoteFile)) {
                    setColor(COLOR_GREEN);
                    cout << "OK\n";
                    setColor(COLOR_DEFAULT);
                }
                else {
                    setColor(COLOR_RED);
                    cout << "ОШИБКА\n";
                    setColor(COLOR_DEFAULT);
                }
            }
        }

        cout << indent << "[ИТОГО] Папок: " << folderCount << ", Файлов: " << fileCount << "\n";
        changeDirectory(oldDir);
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

    bool getFileSize(const string& filename, long long& size) {
        sendCommand("SIZE " + filename);
        string response = receiveResponse();
        if (response.find("213") != string::npos) {
            size = stoll(response.substr(4));
            return true;
        }
        return false;
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

class FTPInterface {
private:
    FTPClient ftp;
    ProfileManager profileManager;
    HistoryManager history;
    bool running;
    int selectedIndex;
    vector<pair<string, bool>> currentFiles;
    string message;
    int messageTimer;

    void showMessage(const string& msg, bool isError = false) {
        message = msg;
        messageTimer = 30;
        history.add(msg);
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
        vector<bool> fieldCompleted = { false, false, false, false, false };

        while (true) {
            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                         ДОБАВЛЕНИЕ НОВОГО ПРОФИЛЯ                          |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);
            cout << "|                                                                            |\n";
            cout << "|  [↑] [↓] Выбор поля    [Enter] Подтвердить    [Tab] Показать пароль        |\n";
            cout << "|  [Esc] Отмена                                                              |\n";
            cout << "|                                                                            |\n";
            cout << "+============================================================================+\n";
            cout << "|                                                                            |\n";

            string fields[] = { "Название: ", "Сервер:   ", "Порт:     ", "Логин:    ", "Пароль:   " };
            string* values[] = { &newProfile.name, &newProfile.server, &newProfile.port,
                                &newProfile.username, &newProfile.password };

            for (int i = 0; i < 5; i++) {
                if (i == currentField) setColor(COLOR_YELLOW);
                cout << "| " << (i == currentField ? ">>" : "  ") << " ";
                setColor(COLOR_DEFAULT);
                cout << fields[i];

                string displayValue;
                if (i == 4 && !showPassword && !newProfile.password.empty())
                    displayValue = string(newProfile.password.length(), '*');
                else
                    displayValue = *values[i];

                if (fieldCompleted[i]) setColor(COLOR_GREEN);
                cout << displayValue;
                setColor(COLOR_DEFAULT);
                if (i == 2 && newProfile.port == "21" && fieldCompleted[i]) cout << " (стандарт)";
                cout << string(40 - displayValue.length(), ' ') << " |\n";
            }

            cout << "|                                                                            |\n";
            cout << "+============================================================================+\n";

            if (!message.empty()) {
                setColor(message.find("Ошибка") != string::npos ? COLOR_RED : COLOR_GREEN);
                cout << "| " << message << string(76 - message.length(), ' ') << " |\n";
                cout << "+============================================================================+\n";
                setColor(COLOR_DEFAULT);
                if (--messageTimer <= 0) message.clear();
            }

            int key = _getch();
            if (key == 224) {
                key = _getch();
                if (key == 72 && currentField > 0) currentField--;
                if (key == 80 && currentField < 4) currentField++;
                continue;
            }
            if (key == 27) return;
            if (key == 9 && currentField == 4) { showPassword = !showPassword; continue; }

            if (key == 13) {
                if (!fieldCompleted[currentField]) {
                    if (values[currentField]->empty()) { showMessage("Заполните поле!", true); continue; }
                    if (currentField == 2 && newProfile.port.empty()) newProfile.port = "21";
                    fieldCompleted[currentField] = true;
                    if (currentField < 4) currentField++;
                    else if (newProfile.isValid()) { profileManager.add(newProfile); showMessage("Профиль добавлен!"); return; }
                }
                continue;
            }

            if (key == 8 && !fieldCompleted[currentField] && !values[currentField]->empty())
                values[currentField]->pop_back();

            if (!fieldCompleted[currentField] && key >= 32 && key <= 126) {
                if (currentField == 2) {
                    if (isdigit(key) && values[currentField]->length() < 5) {
                        if (*values[currentField] == "21") *values[currentField] = "";
                        *values[currentField] += (char)key;
                    }
                }
                else if (values[currentField]->length() < 50)
                    *values[currentField] += (char)key;
            }
        }
    }

    void connectWithProfile(const FTPProfile& profile) {
        system("cls");
        cout << "+============================================================================+\n";
        cout << "|                         ПОДКЛЮЧЕНИЕ К FTP                                  |\n";
        cout << "+============================================================================+\n";
        cout << "| Профиль: " << profile.name << "\n";
        cout << "| Сервер: " << profile.server << "\n";
        cout << "| Порт: " << profile.port << "\n";
        cout << "| Логин: " << profile.username << "\n";
        cout << "| Подключение...\n";
        cout << "+============================================================================+\n";

        if (ftp.connectToServer(profile.server, stoi(profile.port))) {
            cout << "| Авторизация...\n";
            if (ftp.login(profile.username, profile.password)) {
                setColor(COLOR_GREEN);
                cout << "| УСПЕШНО ПОДКЛЮЧЕНО!\n";
                setColor(COLOR_DEFAULT);
                showMessage("Подключено к " + profile.server);
                refreshFileList();
                selectedIndex = 0;
                this_thread::sleep_for(chrono::milliseconds(1000));
                return;
            }
        }
        setColor(COLOR_RED);
        cout << "| ОШИБКА ПОДКЛЮЧЕНИЯ!\n";
        setColor(COLOR_DEFAULT);
        showMessage("Ошибка подключения!", true);
        ftp.resetConnection();
        this_thread::sleep_for(chrono::milliseconds(1500));
    }

    void selectProfile() {
        if (profileManager.isEmpty()) {
            showMessage("Нет профилей. Создайте новый!", true);
            addNewProfile();
            return;
        }

        int currentSelection = 0;
        bool addingNew = false;

        while (true) {
            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                         ВЫБОР ПРОФИЛЯ FTP                                  |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);

            auto profiles = profileManager.getAll();
            for (int i = 0; i < (int)profiles.size(); i++) {
                setColor((i == currentSelection && !addingNew) ? COLOR_YELLOW : COLOR_DEFAULT);
                cout << "| " << ((i == currentSelection && !addingNew) ? ">" : " ") << " [" << (i + 1) << "] "
                    << profiles[i].name << " (" << profiles[i].server << ")\n";
            }

            setColor(addingNew ? COLOR_YELLOW : COLOR_DEFAULT);
            cout << "| " << (addingNew ? ">" : " ") << " [+] Добавить новый профиль\n";
            setColor(COLOR_DEFAULT);

            cout << "+============================================================================+\n";
            cout << "|  [↑] [↓] Выбор    [Enter] Выбрать    [N] Новый    [D] Удалить    [Esc] Выход |\n";
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
                    cout << "\n| Удалить профиль? (y/n): ";
                    if (_getch() == 'y') {
                        profileManager.remove(currentSelection);
                        showMessage("Профиль удален");
                        if (profileManager.isEmpty()) return;
                        if (currentSelection >= (int)profileManager.getAll().size())
                            currentSelection = profileManager.getAll().size() - 1;
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

    void downloadItem() {
        if (!ftp.isConnected()) { showMessage("Подключитесь к серверу!", true); return; }
        if (currentFiles.empty()) { showMessage("Нет элементов для скачивания!", true); return; }

        string name = currentFiles[selectedIndex].first;
        bool isDir = currentFiles[selectedIndex].second;

        if (isDir) {
            // Скачивание папки
            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                         СКАЧИВАНИЕ ПАПКИ                                   |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);
            cout << "| Папка: " << name << "\n";
            cout << "| Локальный путь (Enter - " << name << "): ";

            showCursor();
            string localPath;
            getline(cin, localPath);
            if (localPath.empty()) localPath = name;

            cout << "| Скачивание...\n";
            cout << "+============================================================================+\n\n";

            if (ftp.downloadFolder(name, localPath)) {
                setColor(COLOR_GREEN);
                cout << "\n| ПАПКА УСПЕШНО СКАЧАНА!\n";
                showMessage("Папка " + name + " скачана");
            }
            else {
                setColor(COLOR_RED);
                cout << "\n| ОШИБКА ПРИ СКАЧИВАНИИ!\n";
                showMessage("Ошибка скачивания папки!", true);
            }
            hideCursor();
            this_thread::sleep_for(chrono::milliseconds(3000));
        }
        else {
            // Скачивание файла
            showMessage("Скачивание: " + name);
            if (ftp.downloadFile(name, name)) {
                showMessage("Файл " + name + " скачан!");
            }
            else {
                showMessage("Ошибка скачивания " + name, true);
            }
        }
    }

    void uploadItem() {
        if (!ftp.isConnected()) { showMessage("Подключитесь к серверу!", true); return; }

        // Сначала спрашиваем, что загружать
        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         ВЫБОР ТИПА ЗАГРУЗКИ                                |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "|                                                                            |\n";
        cout << "|   [F] Загрузить файл                                                       |\n";
        cout << "|   [D] Загрузить папку (рекурсивно)                                         |\n";
        cout << "|                                                                            |\n";
        cout << "|   [Esc] Отмена                                                             |\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";

        int choice = _getch();

        if (choice == 'f' || choice == 'F') {
            // Загрузка файла
            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                         ЗАГРУЗКА ФАЙЛА                                     |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);
            cout << "| Имя файла: ";
            showCursor();
            string localFile;
            cin >> localFile;
            string remoteFile = localFile;
            cout << "| Имя на сервере (Enter - то же): ";
            cin.ignore();
            getline(cin, remoteFile);
            if (remoteFile.empty()) remoteFile = localFile;
            cout << "| Загрузка...\n";
            cout << "+============================================================================+\n";

            if (ftp.uploadFile(localFile, remoteFile)) {
                setColor(COLOR_GREEN);
                cout << "| ФАЙЛ ЗАГРУЖЕН!\n";
                showMessage("Файл " + localFile + " загружен");
                refreshFileList();
            }
            else {
                setColor(COLOR_RED);
                cout << "| ОШИБКА ЗАГРУЗКИ!\n";
                showMessage("Ошибка загрузки файла!", true);
            }
            hideCursor();
            this_thread::sleep_for(chrono::milliseconds(1500));
        }
        else if (choice == 'd' || choice == 'D') {
            // Загрузка папки
            if (currentFiles.empty() || !currentFiles[selectedIndex].second) {
                showMessage("Выберите папку на сервере, КУДА загружать!", true);
                hideCursor();
                this_thread::sleep_for(chrono::milliseconds(1500));
                return;
            }

            string remoteFolder = currentFiles[selectedIndex].first;

            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                         ЗАГРУЗКА ПАПКИ НА СЕРВЕР                           |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);
            cout << "| Удаленная папка: " << remoteFolder << "\n";
            cout << "|                                                                            |\n";
            cout << "| Локальная папка (полный путь): ";

            showCursor();
            string localFolder;
            getline(cin, localFolder);

            if (!fs::exists(localFolder) || !fs::is_directory(localFolder)) {
                setColor(COLOR_RED);
                cout << "|                                                                            |\n";
                cout << "| ОШИБКА: Папка не существует или не является директорией!\n";
                setColor(COLOR_DEFAULT);
                showMessage("Папка не найдена!", true);
                hideCursor();
                this_thread::sleep_for(chrono::milliseconds(2000));
                return;
            }

            cout << "|                                                                            |\n";
            cout << "| Загрузка...                                                               |\n";
            cout << "+============================================================================+\n\n";

            if (ftp.uploadFolder(localFolder, remoteFolder)) {
                setColor(COLOR_GREEN);
                cout << "\n| ПАПКА УСПЕШНО ЗАГРУЖЕНА!\n";
                setColor(COLOR_DEFAULT);
                showMessage("Папка " + localFolder + " загружена в " + remoteFolder);
                refreshFileList();
            }
            else {
                setColor(COLOR_RED);
                cout << "\n| ОШИБКА ПРИ ЗАГРУЗКЕ!\n";
                setColor(COLOR_DEFAULT);
                showMessage("Ошибка загрузки папки!", true);
            }
            hideCursor();
            this_thread::sleep_for(chrono::milliseconds(3000));
        }
    }

    void drawExitScreen() {
        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                              ВЫХОД ИЗ ПРОГРАММЫ                            |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "|                                                                            |\n";
        cout << "|                        Вы уверены, что хотите выйти?                       |\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";
        cout << "|                                                                            |\n";
        setColor(COLOR_YELLOW);
        cout << "|                          [Y] Да            [N] Нет                         |\n";
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
        if (!ftp.isConnected()) { showMessage("Подключитесь к серверу!", true); return; }
        if (currentFiles.empty()) { showMessage("Нет файлов!", true); return; }

        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         ПЕРЕИМЕНОВАНИЕ                                     |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);

        string oldName = currentFiles[selectedIndex].first;
        cout << "| Старое имя: " << oldName << "\n";
        cout << "| Новое имя: ";
        showCursor();
        string newName;
        cin >> newName;

        if (!newName.empty() && ftp.renameFile(oldName, newName)) {
            showMessage("Переименовано в " + newName);
            refreshFileList();
        }
        else if (!newName.empty()) showMessage("Ошибка переименования!", true);

        hideCursor();
        this_thread::sleep_for(chrono::milliseconds(1500));
    }

    void handleFileInfo() {
        if (!ftp.isConnected()) { showMessage("Подключитесь к серверу!", true); return; }
        if (currentFiles.empty()) { showMessage("Нет файлов!", true); return; }

        string name = currentFiles[selectedIndex].first;
        bool isDir = currentFiles[selectedIndex].second;

        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         ИНФОРМАЦИЯ                                         |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);
        cout << "| Имя: " << name << "\n";
        cout << "| Тип: " << (isDir ? "Папка" : "Файл") << "\n";

        if (!isDir) {
            long long size;
            if (ftp.getFileSize(name, size)) {
                if (size < 1024) cout << "| Размер: " << size << " байт\n";
                else if (size < 1024 * 1024) cout << "| Размер: " << size / 1024 << " KB\n";
                else cout << "| Размер: " << size / (1024 * 1024) << " MB\n";
            }
        }
        cout << "+============================================================================+\n";
        cout << "| [ЛЮБАЯ КЛАВИША] Назад                                                      |\n";
        cout << "+============================================================================+\n";
        _getch();
    }

public:
    FTPInterface() : running(true), selectedIndex(0), messageTimer(0) {
        profileManager.load();
        refreshFileList();
    }

    void run() {
        setRussianConsole();
        hideCursor();

        while (running) {
            system("cls");
            setColor(COLOR_CYAN);
            cout << "+============================================================================+\n";
            cout << "|                      F T P   K Л И Е Н Т   v1.0                             |\n";
            cout << "|                                                                             |\n";
            cout << "+============================================================================+\n";
            setColor(COLOR_DEFAULT);

            if (ftp.isConnected()) {
                setColor(COLOR_GREEN);
                cout << "| ПОДКЛЮЧЕН\n";
                setColor(COLOR_DEFAULT);
                cout << "| Сервер: " << ftp.getServer() << "\n";
                cout << "| Папка:  " << ftp.getCurrentDir() << "\n";
                cout << "+============================================================================+\n";
                cout << "| ФАЙЛЫ И ПАПКИ (" << currentFiles.size() << " шт.)                         |\n";
                cout << "+============================================================================+\n";

                if (currentFiles.empty()) cout << "|                               ПУСТО                                    |\n";
                else for (int i = 0; i < (int)currentFiles.size() && i < 15; i++) {
                    setColor(i == selectedIndex ? COLOR_YELLOW : COLOR_DEFAULT);
                    cout << "| " << (i == selectedIndex ? ">" : " ") << " ";
                    setColor(currentFiles[i].second ? COLOR_CYAN : COLOR_GREEN);
                    cout << (currentFiles[i].second ? "[ПАПКА]" : "[ФАЙЛ] ");
                    setColor(COLOR_DEFAULT);
                    string name = currentFiles[i].first;
                    if (name.length() > 38) name = name.substr(0, 35) + "...";
                    cout << left << setw(38) << name;
                    if (i == selectedIndex) { setColor(COLOR_YELLOW); cout << " <"; }
                    cout << " |\n";
                }

                cout << "+============================================================================+\n";
                cout << "|                                                                            |\n";
                cout << "| КОМАНДЫ (СТРЕЛКИ [↑] [↓] - НАВИГАЦИЯ):                                     |\n";
                cout << "|                                                                            |\n";
                cout << "|   [Enter] Открыть папку    [Backspace] Назад                               |\n";
                cout << "|   [D] Скачать (файл/папку) [U] Загрузить (файл/папку)                      |\n";
                cout << "|   [T] Удалить              [C] Создать папку      [N] Переименовать        |\n";
                cout << "|   [I] Информация           [R] Обновить           [F] Отключиться          |\n";
                cout << "|   [H] История              [Q] Выход                                      |\n";
                cout << "|                                                                            |\n";
                cout << "|   * При загрузке/скачивании папки автоматически определяется тип          |\n";
                cout << "|   * Для скачивания: если выбрана папка - скачается рекурсивно              |\n";
                cout << "|   * Для загрузки: выберите тип (F - файл, D - папка)                       |\n";
                cout << "|                                                                            |\n";
                cout << "+============================================================================+\n";

            }
            else {
                cout << "| [НЕ ПОДКЛЮЧЕН]                                                              |\n";
                cout << "+============================================================================+\n";
                cout << "|                                                                            |\n";
                cout << "| КОМАНДЫ:                                                                   |\n";
                cout << "|                                                                            |\n";
                cout << "|   [Enter] Выбрать профиль    [H] История    [Q] Выход                       |\n";
                cout << "|                                                                            |\n";
                cout << "+============================================================================+\n";
            }

            if (!message.empty()) {
                setColor(message.find("Ошибка") != string::npos ? COLOR_RED : COLOR_GREEN);
                cout << "| " << message << string(76 - message.length(), ' ') << " |\n";
                cout << "+============================================================================+\n";
                setColor(COLOR_DEFAULT);
                if (--messageTimer <= 0) message.clear();
            }

            int key = _getch();

            if (!ftp.isConnected()) {
                switch (key) {
                case 13: selectProfile(); break;
                case 'h': case 'H': history.show(); break;
                case 'q': case 'Q': drawExitScreen(); break;
                }
            }
            else {
                switch (key) {
                case 224: // Стрелки
                    key = _getch();
                    if (key == 72 && selectedIndex > 0) selectedIndex--;
                    if (key == 80 && selectedIndex < (int)currentFiles.size() - 1) selectedIndex++;
                    break;

                case 13: // Enter - открыть папку
                    if (!currentFiles.empty() && currentFiles[selectedIndex].second) {
                        string folderName = currentFiles[selectedIndex].first;
                        if (ftp.changeDirectory(folderName)) {
                            refreshFileList();
                            selectedIndex = 0;
                            showMessage("Открыта папка: " + folderName);
                        }
                        else showMessage("Не удалось открыть папку!", true);
                    }
                    break;

                case 8: // Backspace - назад
                    if (ftp.changeDirectory("..")) {
                        refreshFileList();
                        selectedIndex = 0;
                        showMessage("Назад");
                    }
                    break;

                case 'd': case 'D': // Скачать (файл или папку)
                    downloadItem();
                    break;

                case 'u': case 'U': // Загрузить (с выбором типа)
                    uploadItem();
                    break;

                case 't': case 'T': // Удалить
                    if (!currentFiles.empty()) {
                        string name = currentFiles[selectedIndex].first;
                        bool isDir = currentFiles[selectedIndex].second;
                        cout << "\n| Удалить " << (isDir ? "папку" : "файл") << "? (y/n): ";
                        if (_getch() == 'y') {
                            if ((isDir && ftp.deleteDirectory(name)) || (!isDir && ftp.deleteFile(name))) {
                                showMessage("Удалено: " + name);
                                refreshFileList();
                            }
                            else showMessage("Ошибка удаления!", true);
                        }
                    }
                    break;

                case 'c': case 'C': // Создать папку
                {
                    system("cls");
                    setColor(COLOR_CYAN);
                    cout << "+============================================================================+\n";
                    cout << "|                         СОЗДАНИЕ ПАПКИ                                   |\n";
                    cout << "+============================================================================+\n";
                    setColor(COLOR_DEFAULT);
                    cout << "| Имя папки (Enter - отмена): ";
                    showCursor();
                    string dirname;
                    getline(cin, dirname);
                    if (!dirname.empty() && ftp.createDirectory(dirname)) {
                        setColor(COLOR_GREEN);
                        cout << "| ПАПКА СОЗДАНА!\n";
                        showMessage("Папка создана");
                        refreshFileList();
                    }
                    else if (!dirname.empty()) {
                        setColor(COLOR_RED);
                        cout << "| ОШИБКА СОЗДАНИЯ!\n";
                        showMessage("Ошибка создания!", true);
                    }
                    hideCursor();
                    this_thread::sleep_for(chrono::milliseconds(1500));
                }
                break;

                case 'n': case 'N': handleRename(); break;
                case 'i': case 'I': handleFileInfo(); break;
                case 'r': case 'R': refreshFileList(); showMessage("Обновлено"); break;
                case 'h': case 'H': history.show(); break;
                case 'f': case 'F':
                    ftp.disconnect();
                    currentFiles.clear();
                    selectedIndex = 0;
                    showMessage("Отключено");
                    break;
                case 'q': case 'Q': drawExitScreen(); break;
                }
            }
        }

        system("cls");
        setColor(COLOR_CYAN);
        cout << "\n\n\t\t+=====================================+\n";
        cout << "\t\t|         ДО СВИДАНИЯ!                |\n";
        cout << "\t\t+=====================================+\n\n";
        setColor(COLOR_DEFAULT);
        this_thread::sleep_for(chrono::milliseconds(1000));
    }
};

int main() {
    FTPInterface app;
    app.run();
    return 0;
}