#pragma once
#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <vector>

using namespace std;

class FileEditor {
private:
    string filename;
    vector<string> lines;
    int cursorX, cursorY;
    bool modified;

    void loadContent();
    void saveContent();
    void drawEditor();
    void insertChar(char ch);
    void deleteChar();

public:
    FileEditor(const string& filename);
    void run();
    bool isModified() const { return modified; }
};

#endif