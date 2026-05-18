#include "editor.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <conio.h>

FileEditor::FileEditor(const string& fname) : filename(fname), cursorX(0), cursorY(0), modified(false) {
    ifstream file(filename);
    if (file.is_open()) {
        stringstream ss;
        ss << file.rdbuf();
        string content = ss.str();
        stringstream contentSs(content);
        string line;
        while (getline(contentSs, line)) {
            lines.push_back(line);
        }
        if (lines.empty()) lines.push_back("");
        file.close();
    }
}

void FileEditor::saveContent() {
    ofstream file(filename);
    if (file.is_open()) {
        for (const auto& line : lines) {
            file << line << "\n";
        }
        file.close();
        modified = false;
    }
}

void FileEditor::drawEditor() {
    system("cls");
    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "|                         FILE EDITOR                                        |\n";
    cout << "| " << filename << "\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_DEFAULT);

    int startLine = max(0, cursorY - 10);
    int endLine = min((int)lines.size(), startLine + 20);

    for (int i = startLine; i < endLine; i++) {
        if (i == cursorY) setColor(COLOR_YELLOW);
        cout << (i == cursorY ? "> " : "  ");
        setColor(COLOR_DEFAULT);

        string line = lines[i];
        if (line.length() > 70) line = line.substr(0, 70);
        cout << line;
        if (i == cursorY) cout << "_";
        cout << string(max(0, 70 - (int)line.length()), ' ') << " |\n";
    }

    setColor(COLOR_CYAN);
    cout << "+============================================================================+\n";
    cout << "| Line: " << cursorY + 1 << "/" << lines.size() << " | "
        << (modified ? "[MODIFIED]" : "[SAVED]") << "\n";
    cout << "+============================================================================+\n";
    setColor(COLOR_YELLOW);
    cout << "| [Ctrl+S] Save  [Ctrl+X] Exit  [Ins] New Line  [Del] Delete                  |\n";
    setColor(COLOR_DEFAULT);
    cout << "+============================================================================+\n";
}

void FileEditor::insertChar(char ch) {
    if (cursorX < (int)lines[cursorY].length()) {
        lines[cursorY].insert(cursorX, 1, ch);
    }
    else {
        lines[cursorY] += ch;
    }
    cursorX++;
    modified = true;
}

void FileEditor::deleteChar() {
    if (cursorX < (int)lines[cursorY].length()) {
        lines[cursorY].erase(cursorX, 1);
        modified = true;
    }
    else if (cursorY < (int)lines.size() - 1) {
        lines[cursorY] += lines[cursorY + 1];
        lines.erase(lines.begin() + cursorY + 1);
        modified = true;
    }
}

void FileEditor::run() {
    showCursor();

    while (true) {
        drawEditor();

        int key = _getch();

        if (key == 224) {
            key = _getch();
            switch (key) {
            case 72: if (cursorY > 0) cursorY--; break;
            case 80: if (cursorY < (int)lines.size() - 1) cursorY++; break;
            case 75: if (cursorX > 0) cursorX--; break;
            case 77: if (cursorX < (int)lines[cursorY].length()) cursorX++; break;
            case 83: deleteChar(); break;
            case 82: lines.insert(lines.begin() + cursorY + 1, ""); modified = true; break;
            }
        }
        else if (key == 13) {
            lines.insert(lines.begin() + cursorY + 1, lines[cursorY].substr(cursorX));
            lines[cursorY] = lines[cursorY].substr(0, cursorX);
            cursorY++;
            cursorX = 0;
            modified = true;
        }
        else if (key == 8) {
            if (cursorX > 0) {
                cursorX--;
                deleteChar();
            }
            else if (cursorY > 0) {
                cursorX = lines[cursorY - 1].length();
                lines[cursorY - 1] += lines[cursorY];
                lines.erase(lines.begin() + cursorY);
                cursorY--;
                modified = true;
            }
        }
        else if (key == 19) { // Ctrl+S
            saveContent();
        }
        else if (key == 24) { // Ctrl+X
            break;
        }
        else if (key >= 32 && key <= 126) {
            insertChar((char)key);
        }
    }

    hideCursor();
}