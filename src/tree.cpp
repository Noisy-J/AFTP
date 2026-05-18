#include "tree.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <conio.h>

static void printTree(FTPClient& ftp, const string& path, int level) {
    string indent = string(level * 2, ' ');

    string oldDir = ftp.getCurrentDir();
    if (!ftp.changeDirectory(path)) return;

    auto files = ftp.listFiles();
    for (size_t i = 0; i < files.size(); i++) {
        bool isLast = (i == files.size() - 1);
        string prefix = indent;
        if (level > 0) {
            prefix += isLast ? "└── " : "├── ";
        }

        if (files[i].isDirectory) {
            setColor(COLOR_CYAN);
            cout << prefix << "DIR: " << files[i].name << "\n";
            setColor(COLOR_DEFAULT);
            printTree(ftp, files[i].name, level + 1);
        }
        else {
            setColor(COLOR_GREEN);
            cout << prefix << "FILE: " << files[i].name << "\n";
            setColor(COLOR_DEFAULT);
        }
    }

    ftp.changeDirectory(oldDir);
}

void showTreeView(FTPClient& ftp, const vector<FTPFile>& files, const string& currentPath) {
    system("cls");
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         FILE TREE                                          |\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);
    cout << "| Path: " << currentPath << "\n";
    cout << "+============================================================================+\n\n";

    string oldDir = ftp.getCurrentDir();

    for (size_t i = 0; i < files.size(); i++) {
        bool isLast = (i == files.size() - 1);
        string prefix = isLast ? "└── " : "├── ";

        if (files[i].isDirectory) {
            setColor(COLOR_CYAN);
            cout << prefix << "DIR: " << files[i].name << "\n";
            setColor(COLOR_DEFAULT);
            printTree(ftp, files[i].name, 1);
        }
        else {
            setColor(COLOR_GREEN);
            cout << prefix << "FILE: " << files[i].name << "\n";
            setColor(COLOR_DEFAULT);
        }
    }

    ftp.changeDirectory(oldDir);

    cout << "\n+============================================================================+\n";
    cout << "| [E] Export to file  [Any key] Back                                         |\n";
    cout << "+============================================================================+\n";

    int key = _getch();
    if (key == 'e' || key == 'E') {
        ofstream file("tree_output.txt");
        if (file.is_open()) {
            file << "File tree for: " << currentPath << "\n\n";
            file.close();
        }
        setColor(COLOR_GREEN);
        cout << "\n| Exported to tree_output.txt\n";
        setColor(COLOR_DEFAULT);
        Sleep(1000);
    }
}