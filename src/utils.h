#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <string>
#include <vector>
#include <map>

using namespace std;

// Цвета
extern int COLOR_DEFAULT;
extern int COLOR_GREEN;
extern int COLOR_RED;
extern int COLOR_YELLOW;
extern int COLOR_CYAN;
extern int COLOR_WHITE;
extern int COLOR_BLUE;

void setColor(int color);
void setRussianConsole();
void hideCursor();
void showCursor();
void gotoxy(int x, int y);

// Управление темой
bool loadTheme(const string& themeName);
vector<string> getAvailableThemes();
string getCurrentTheme();
void setCurrentTheme(const string& themeName);

// Управление языком
bool loadLanguage(const string& langCode);
string getString(const string& key);
vector<string> getAvailableLanguages();
string getCurrentLanguage();
void setCurrentLanguage(const string& langCode);

// Сохранение настроек
void saveSettings();
void loadSettings();

#endif