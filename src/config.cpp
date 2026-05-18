#include "config.h"
#include "utils.h"
#include <iostream>
#include <conio.h>

void showSettingsMenu() {
    while (true) {
        system("cls");
        setColor(COLOR_CYAN);
        cout << "+============================================================================+\n";
        cout << "|                         SETTINGS                                          |\n";
        cout << "+============================================================================+\n";
        setColor(COLOR_DEFAULT);

        vector<string> langs = getAvailableLanguages();
        vector<string> themes = getAvailableThemes();

        cout << "|                                                                            |\n";
        cout << "| 1. Language: " << getCurrentLanguage() << "\n";
        cout << "|    Available: ";
        for (const auto& l : langs) cout << l << " ";
        cout << "\n";
        cout << "|                                                                            |\n";
        cout << "| 2. Theme: " << getCurrentTheme() << "\n";
        cout << "|    Available: ";
        for (const auto& t : themes) cout << t << " ";
        cout << "\n";
        cout << "|                                                                            |\n";
        cout << "+============================================================================+\n";
        cout << "| [1] Change Language  [2] Change Theme  [Enter] Save  [Esc] Cancel          |\n";
        cout << "+============================================================================+\n";

        int key = _getch();
        if (key == 27) break;
        if (key == 13) {
            saveSettings();
            setColor(COLOR_GREEN);
            cout << "\n| Settings saved!\n";
            setColor(COLOR_DEFAULT);
            Sleep(1000);
            break;
        }
        if (key == '1') {
            int idx = 0;
            for (size_t i = 0; i < langs.size(); i++) {
                if (langs[i] == getCurrentLanguage()) idx = i;
            }
            idx = (idx + 1) % langs.size();
            setCurrentLanguage(langs[idx]);
        }
        if (key == '2') {
            int idx = 0;
            for (size_t i = 0; i < themes.size(); i++) {
                if (themes[i] == getCurrentTheme()) idx = i;
            }
            idx = (idx + 1) % themes.size();
            setCurrentTheme(themes[idx]);
        }
    }
}

void initConfig() {
    loadSettings();
    loadTheme(getCurrentTheme());
    loadLanguage(getCurrentLanguage());
}