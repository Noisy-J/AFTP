#include "utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int COLOR_DEFAULT = 7;
int COLOR_GREEN = 10;
int COLOR_RED = 12;
int COLOR_YELLOW = 14;
int COLOR_CYAN = 11;
int COLOR_WHITE = 15;
int COLOR_BLUE = 9;

static string currentTheme = "dark";
static string currentLanguage = "ru_RU";
static map<string, map<string, string>> translations;
static map<string, string> currentStrings;

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

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

void gotoxy(int x, int y) {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

bool loadTheme(const string& themeName) {
    string path = "themes/" + themeName + ".theme";
    ifstream file(path);
    if (!file.is_open()) return false;

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != string::npos) {
            string key = line.substr(0, pos);
            string val = line.substr(pos + 1);
            if (key == "default") COLOR_DEFAULT = stoi(val);
            else if (key == "green") COLOR_GREEN = stoi(val);
            else if (key == "red") COLOR_RED = stoi(val);
            else if (key == "yellow") COLOR_YELLOW = stoi(val);
            else if (key == "cyan") COLOR_CYAN = stoi(val);
            else if (key == "white") COLOR_WHITE = stoi(val);
            else if (key == "blue") COLOR_BLUE = stoi(val);
        }
    }
    currentTheme = themeName;
    return true;
}

vector<string> getAvailableThemes() {
    vector<string> themes;
    for (const auto& entry : fs::directory_iterator("themes")) {
        if (entry.path().extension() == ".theme") {
            themes.push_back(entry.path().stem().string());
        }
    }
    return themes;
}

string getCurrentTheme() { return currentTheme; }
void setCurrentTheme(const string& themeName) { loadTheme(themeName); }

bool loadLanguage(const string& langCode) {
    string path = "lang/" + langCode + ".lang";
    ifstream file(path);
    if (!file.is_open()) return false;

    map<string, string> strings;
    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != string::npos) {
            strings[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    translations[langCode] = strings;
    if (langCode == currentLanguage) {
        currentStrings = strings;
    }
    return true;
}

string getString(const string& key) {
    if (currentStrings.find(key) != currentStrings.end()) {
        return currentStrings[key];
    }
    return key;
}

vector<string> getAvailableLanguages() {
    vector<string> langs;
    for (const auto& entry : fs::directory_iterator("lang")) {
        if (entry.path().extension() == ".lang") {
            langs.push_back(entry.path().stem().string());
        }
    }
    return langs;
}

string getCurrentLanguage() { return currentLanguage; }

void setCurrentLanguage(const string& langCode) {
    if (translations.find(langCode) != translations.end()) {
        currentStrings = translations[langCode];
        currentLanguage = langCode;
    }
    else if (loadLanguage(langCode)) {
        currentStrings = translations[langCode];
        currentLanguage = langCode;
    }
}

void saveSettings() {
    ofstream file("aftp_settings.ini");
    if (file.is_open()) {
        file << "theme=" << currentTheme << "\n";
        file << "language=" << currentLanguage << "\n";
        file.close();
    }
}

void loadSettings() {
    ifstream file("aftp_settings.ini");
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != string::npos) {
                string key = line.substr(0, pos);
                string val = line.substr(pos + 1);
                if (key == "theme") loadTheme(val);
                if (key == "language") setCurrentLanguage(val);
            }
        }
        file.close();
    }
}