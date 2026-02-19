#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <conio.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <shellapi.h>
#include <unordered_map>
#include <filesystem>
#include <unordered_set>
#include <locale>
#include <codecvt>

using namespace std;
namespace fs = std::filesystem;

// Функция для установки кодировки консоли на CP866
void setConsoleToCP866() {
    SetConsoleOutputCP(866); 
    SetConsoleCP(866);       
}

// Функция для преобразования строки из UTF-16 в CP866
std::string utf16_to_cp866(const std::wstring& utf16_str) {
    if (utf16_str.empty()) return "";

    // Получаем размер буфера для строки в CP866
    int cp866_size = WideCharToMultiByte(866, 0, utf16_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (cp866_size == 0) {
        return "";
    }

    // Создаем строку для хранения результата
    std::string cp866_str(cp866_size, 0);
    WideCharToMultiByte(866, 0, utf16_str.c_str(), -1, &cp866_str[0], cp866_size, nullptr, nullptr);

    // Удаляем завершающий нулевой символ
    cp866_str.pop_back();

    return cp866_str;
}

// Функция для преобразования строки из UTF-8 в CP866
string utf8_to_cp866(const string& utf8_str) {
    // Преобразуем UTF-8 в UTF-16
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wide_size == 0) {
        return "";
    }

    wstring wide_str(wide_size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide_str[0], wide_size);

    // Преобразуем UTF-16 в CP866
    int cp866_size = WideCharToMultiByte(866, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (cp866_size == 0) {
        return "";
    }

    string cp866_str(cp866_size, 0);
    WideCharToMultiByte(866, 0, wide_str.c_str(), -1, &cp866_str[0], cp866_size, nullptr, nullptr);

    // Удаляем завершающий нулевой символ
    cp866_str.pop_back();

    return cp866_str;
}

// Списки расширений для исполняемых файлов и исходных кодов
const vector<string> executableExtensions = { "exe", "bat", "cmd" };
const vector<string> sourceFileExtensions = { "cpp", "h", "html", "js", "css", "py", "java", "php", "xml", "json" };

// Функция для установки цвета текста в консоли
void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Функция для сброса цвета текста в консоли
void resetColor() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
}

// Функция для подсчета количества строк в файле
int countLinesInFile(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        return 0;
    }

    int lineCount = 0;
    string line;
    while (getline(file, line)) {
        lineCount++;
    }
    file.close();
    return lineCount;
}

// Структура для хранения информации о файле
struct FileInfo {
    string name;            // Имя файла
    bool isDirectory;       // Является ли файл директорией
    string extension;       // Расширение файла
    WIN32_FIND_DATAW fileData; // Данные о файле
    bool isSelected;        // Выбран ли файл
};

// Кэш для хранения выбранных файлов
unordered_map<string, bool> selectionCache;

// Функция для получения списка дисков
vector<FileInfo> getDrives() {
    vector<FileInfo> drives;
    DWORD driveMask = GetLogicalDrives();

    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        if (driveMask & 1) {
            FileInfo driveInfo;
            driveInfo.name = string(1, drive) + ":\\";
            driveInfo.isDirectory = true;
            driveInfo.extension = "";
            driveInfo.isSelected = false;
            drives.push_back(driveInfo);
        }
        driveMask >>= 1;
    }

    return drives;
}

// Функция для получения списка файлов в директории
vector<FileInfo> getFilesInDirectory(const string& path) {
    vector<FileInfo> files;
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW((wstring(path.begin(), path.end()) + L"\\*").c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
                FileInfo fileInfo;
                fileInfo.name = utf16_to_cp866(findFileData.cFileName); // Преобразуем имя файла в CP866
                fileInfo.isDirectory = (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                fileInfo.extension = fileInfo.isDirectory ? "" : fileInfo.name.substr(fileInfo.name.find_last_of(".") + 1);
                fileInfo.fileData = findFileData; // Сохраняем данные о файле
                fileInfo.isSelected = false;
                files.push_back(fileInfo);
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
    }

    // Сортируем файлы (сначала директории, затем файлы)
    sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        string aName = a.name;
        string bName = b.name;
        transform(aName.begin(), aName.end(), aName.begin(), ::tolower);
        transform(bName.begin(), bName.end(), bName.begin(), ::tolower);

        if (a.isDirectory && !b.isDirectory) return true;
        if (!a.isDirectory && b.isDirectory) return false;

        if (!a.isDirectory && !b.isDirectory) {
            bool aIsExecutable = find(executableExtensions.begin(), executableExtensions.end(), a.extension) != executableExtensions.end();
            bool bIsExecutable = find(executableExtensions.begin(), executableExtensions.end(), b.extension) != executableExtensions.end();
            if (aIsExecutable && !bIsExecutable) return true;
            if (!aIsExecutable && bIsExecutable) return false;
        }

        if (a.extension != b.extension) return a.extension < b.extension;
        return aName < bName;
    });

    return files;
}

// Перечисление для определения языка программирования
enum class Language {
    HTML,
    JavaScript,
    CPP,
    Python,
    Rebol,
    Unknown
};

// Структура для хранения цветов синтаксической подсветки
struct SyntaxColor {
    const int COMMENT = 8;      // Серый
    const int KEYWORD = 14;     // Желтый
    const int STRING = 12;      // Красный
    const int NUMBER = 10;      // Зеленый
    const int TAG = 9;          // Синий
    const int ATTRIBUTE = 13;   // Фиолетовый
    const int OPERATOR = 11;    // Голубой
    const int FUNCTION = 15;    // Белый
    const int PREPROCESSOR = 3; // Бирюзовый
};

// Функция для подсветки синтаксиса в зависимости от языка
void highlightSyntax(const string& line, Language lang) {
    SyntaxColor colors;

    unordered_set<string> keywords;
    unordered_set<string> operators;

    switch(lang) {
        case Language::HTML:
            keywords = {"html", "head", "body", "div", "span", "p", "a", "img", "script", 
                       "style", "link", "meta", "title", "form", "input", "button"};
            break;

        case Language::JavaScript:
            keywords = {"function", "var", "let", "const", "if", "else", "return", 
                       "class", "new", "this", "for", "while", "do", "break", "continue",
                       "try", "catch", "throw", "async", "await"};
            operators = {"+", "-", "*", "/", "=", "==", "===", "!=", "!=="};
            break;

        case Language::CPP:
            keywords = {"int", "char", "bool", "float", "double", "void", "class",
                       "struct", "enum", "union", "const", "static", "virtual",
                       "public", "private", "protected", "template", "typename",
                       "if", "else", "for", "while", "do", "switch", "case",
                       "break", "continue", "return", "new", "delete"};
            operators = {"::", "->", ".", "+", "-", "*", "/", "=", "==", "!=", "<", ">"};
            break;

        case Language::Python:
            keywords = {"def", "class", "if", "else", "elif", "for", "while",
                       "try", "except", "finally", "with", "as", "import",
                       "from", "return", "yield", "lambda", "global", "nonlocal",
                       "True", "False", "None"};
            operators = {"+", "-", "*", "/", "//", "**", "=", "==", "!=", "<", ">"};
            break;

        case Language::Rebol:
            keywords = {"rebol", "red", "do", "if", "either", "unless", "while",
                       "until", "loop", "repeat", "foreach", "forall", "remove-each",
                       "func", "function", "does", "has", "make", "to"};
            operators = {"+", "-", "*", "/", "=", "==", "<>", "<", ">"};
            break;

        default:
            break;
    }

    size_t pos = 0;

    // Подсветка комментариев для C++ и JavaScript
    if (lang == Language::CPP || lang == Language::JavaScript) {
        if ((pos = line.find("//")) != string::npos) {
            cout << line.substr(0, pos);
            setColor(colors.COMMENT);
            cout << line.substr(pos);
            resetColor();
            return;
        }
    } else if (lang == Language::Python) {
        if ((pos = line.find("#")) != string::npos) {
            cout << line.substr(0, pos);
            setColor(colors.COMMENT);
            cout << line.substr(pos);
            resetColor();
            return;
        }
    }

    // Подсветка синтаксиса
    for (size_t i = 0; i < line.length(); i++) {
        if (lang == Language::HTML && line[i] == '<') {
            setColor(colors.TAG);
            cout << line[i];
            i++;
            bool isClosingTag = (i < line.length() && line[i] == '/');
            while (i < line.length() && line[i] != '>') {
                if (line[i] == ' ' && !isClosingTag) {
                    resetColor();
                    cout << line[i++];
                    while (i < line.length() && line[i] != '>' && line[i] != '=') {
                        setColor(colors.ATTRIBUTE);
                        cout << line[i++];
                    }
                    continue;
                }
                cout << line[i++];
            }
            if (i < line.length()) cout << line[i];
            resetColor();
            continue;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            setColor(colors.STRING);
            cout << line[i];
            i++;
            while (i < line.length() && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < line.length()) {
                    cout << line[i] << line[i + 1];
                    i += 2;
                    continue;
                }
                cout << line[i];
                i++;
            }
            if (i < line.length()) cout << line[i];
            resetColor();
            continue;
        }

        if (isdigit(line[i])) {
            setColor(colors.NUMBER);
            while (i < line.length() && (isdigit(line[i]) || line[i] == '.' || 
                   line[i] == 'x' || line[i] == 'b' || 
                   (line[i] >= 'a' && line[i] <= 'f') ||
                   (line[i] >= 'A' && line[i] <= 'F'))) {
                cout << line[i];
                i++;
            }
            i--;
            resetColor();
            continue;
        }

        if (lang == Language::CPP && line[i] == '#') {
            setColor(colors.PREPROCESSOR);
            while (i < line.length() && !isspace(line[i])) {
                cout << line[i];
                i++;
            }
            i--;
            resetColor();
            continue;
        }

        bool operatorFound = false;
        for (const auto& op : operators) {
            if (i + op.length() <= line.length() && 
                line.substr(i, op.length()) == op) {
                setColor(colors.OPERATOR);
                cout << op;
                i += op.length() - 1;
                resetColor();
                operatorFound = true;
                break;
            }
        }
        if (operatorFound) continue;

        bool keywordFound = false;
        for (const auto& keyword : keywords) {
            if (i + keyword.length() <= line.length() && 
                line.substr(i, keyword.length()) == keyword &&
                (i == 0 || !isalnum(line[i-1])) && 
                (i + keyword.length() == line.length() || !isalnum(line[i + keyword.length()]))) {

                setColor(colors.KEYWORD);
                cout << keyword;
                i += keyword.length() - 1;
                resetColor();
                keywordFound = true;
                break;
            }
        }
        if (keywordFound) continue;

        cout << line[i];
    }
}

// Функция для отображения номеров строк в файле
void displayLineNumbers(int startY, int lineCount, int fileScrollOffset, int consoleHeight) {
    for (int i = 0; i < consoleHeight - 1; ++i) {
        COORD coord = { 0, (SHORT)(startY + i) };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        setColor(1); // Синий цвет для номеров строк
        if (fileScrollOffset + i < lineCount) {
            cout << setw(4) << fileScrollOffset + i + 1 << " ";
        } else {
            cout << "     "; // Пустое место, если строки закончились
        }
        resetColor();
    }
}

// Функция для определения языка программирования по расширению файла
Language getLanguageByExtension(const string& extension) {
    if (extension == "cpp" || extension == "c" || extension == "h" || extension == "hpp") {
        return Language::CPP;
    } else if (extension == "html" || extension == "htm") {
        return Language::HTML;
    } else if (extension == "js") {
        return Language::JavaScript;
    } else if (extension == "py") {
        return Language::Python;
    } else if (extension == "r") {
        return Language::Rebol;
    } else {
        return Language::Unknown;
    }
}

// Функция для отображения содержимого файла
void displayFileContent(const string& path, int startX, int startY, int width, int height, int fileScrollOffset, bool fullScreenFileView) {
    ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    string line;
    int lineNumber = 0;
    int totalLines = 0;

    while (getline(file, line)) {
        totalLines++;
    }
    file.clear();
    file.seekg(0, ios::beg);

    if (fullScreenFileView) {
        COORD coord = { 0, 0 };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        cout << utf8_to_cp866("Файл: " + path + string(width - path.length() - 6, ' '));
    }

    if (fullScreenFileView) {
        displayLineNumbers(startY, totalLines, fileScrollOffset, height);
    }

    size_t dotPos = path.find_last_of(".");
    string extension = dotPos != string::npos ? path.substr(dotPos + 1) : "";
    Language lang = getLanguageByExtension(extension);

    while (getline(file, line)) {
        if (lineNumber >= fileScrollOffset && lineNumber < fileScrollOffset + height) {
            COORD coord = { (SHORT)(startX + (fullScreenFileView ? 5 : 0)), (SHORT)(startY + lineNumber - fileScrollOffset) };
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
            highlightSyntax(utf8_to_cp866(line.substr(0, width - (fullScreenFileView ? 5 : 0))), lang);
        }
        lineNumber++;
    }
    file.close();
}

// Функция для отображения колонки с файлами
void displayColumn(const vector<FileInfo>& files, int startX, int startY, int width, int height, int cursorPos, int scrollOffset, const string& searchText) {
    for (int i = 0; i < height && i + scrollOffset < files.size(); ++i) {
        COORD coord = { (SHORT)startX, (SHORT)(startY + i) };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

        if (i + scrollOffset == cursorPos) {
            setColor(30);
            cout << "> ";
        } else {
            cout << "  ";
        }

        const FileInfo& fileInfo = files[i + scrollOffset];
        string fileName = fileInfo.name; // Уже в CP866
        string lowerFileName = fileName;
        transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);

        if (fileInfo.isSelected) {
            setColor(14);
        } else if (fileInfo.isDirectory) {
            setColor(15);
        } else if (find(executableExtensions.begin(), executableExtensions.end(), fileInfo.extension) != executableExtensions.end()) {
            setColor(11);
        } else if (find(sourceFileExtensions.begin(), sourceFileExtensions.end(), fileInfo.extension) != sourceFileExtensions.end()) {
            setColor(2);
        } else {
            setColor(8);
        }

        size_t pos = lowerFileName.find(searchText);
        if (pos != string::npos) {
            cout << fileName.substr(0, pos);
            setColor(10);
            cout << fileName.substr(pos, searchText.length());
            resetColor();
            setColor(fileInfo.isSelected ? 14 : (fileInfo.isDirectory ? 15 : (find(executableExtensions.begin(), executableExtensions.end(), fileInfo.extension) != executableExtensions.end()) ? 11 : (find(sourceFileExtensions.begin(), sourceFileExtensions.end(), fileInfo.extension) != sourceFileExtensions.end()) ? 2 : 8));
            cout << fileName.substr(pos + searchText.length());
        } else {
            cout << fileName;
        }

        resetColor();

        if (fileInfo.isDirectory) {
            cout << " /";
        }
    }
}

// Функция для отображения информации о файле
void displayFileInfo(const FileInfo& fileInfo, int startX, int startY, int width, int height) {
    // Очистка области вывода
    for (int i = 0; i < height; ++i) {
        COORD coord = { (SHORT)startX, (SHORT)(startY + i) };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        cout << string(width, ' ');
    }

    COORD coord = { (SHORT)startX, (SHORT)startY };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    cout << utf8_to_cp866("Имя: ") << fileInfo.name << "\n";

    coord.Y++;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    cout << utf8_to_cp866("Размер: " + to_string(fileInfo.fileData.nFileSizeLow) + " байт\n");

    coord.Y++;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    FILETIME creationTime = fileInfo.fileData.ftCreationTime;
    SYSTEMTIME sysTime;
    FileTimeToSystemTime(&creationTime, &sysTime);
    cout << utf8_to_cp866("Дата создания: " + to_string(sysTime.wDay) + "." + to_string(sysTime.wMonth) + "." + to_string(sysTime.wYear) + "\n");

    coord.Y++;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    FILETIME lastWriteTime = fileInfo.fileData.ftLastWriteTime;
    FileTimeToSystemTime(&lastWriteTime, &sysTime);
    cout << utf8_to_cp866("Дата изменения: " + to_string(sysTime.wDay) + "." + to_string(sysTime.wMonth) + "." + to_string(sysTime.wYear) + "\n");

    coord.Y++;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    cout << utf8_to_cp866("Атрибуты: ");
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) cout << utf8_to_cp866("Только для чтения ");
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) cout << utf8_to_cp866("Скрытый ");
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) cout << utf8_to_cp866("Системный ");
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) cout << utf8_to_cp866("Архивный ");
    cout << "\n";
}

// Функция для получения родительской директории
string getParentDirectory(const string& path) {
    size_t pos = path.find_last_of("\\");
    if (pos == string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

// Функция для запуска исполняемого файла
void launchExecutable(const string& path) {
    ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// Функция для открытия командной строки в текущей директории
void openCmdInCurrentDirectory(const string& path) {
    string command = "start cmd /K \"cd /d " + path + "\"";
    system(command.c_str());
}

// Функция для открытия файла в блокноте
void openFileInNotepad(const string& path) {
    ShellExecute(NULL, "open", "notepad.exe", path.c_str(), NULL, SW_SHOWNORMAL);
}

// Функция для отображения закладок
void displayBookmarks(const vector<string>& bookmarks, bool showSavePrompt) {
    system("cls");
    cout << utf8_to_cp866("Закладки:\n");
    for (size_t i = 0; i < bookmarks.size(); ++i) {
        cout << utf8_to_cp866(to_string(i + 1) + ": ");
        if (bookmarks[i].empty()) {
            cout << utf8_to_cp866("<Пусто>\n");
        } else {
            cout << bookmarks[i] << "\n";
        }
    }
    if (showSavePrompt) {
        cout << utf8_to_cp866("Нажмите цифру от 1 до 9, чтобы сохранить текущий путь как закладку.\n");
    } else {
        cout << utf8_to_cp866("Нажмите цифру от 1 до 9, чтобы перейти к закладке.\n");
    }
    cout << utf8_to_cp866("Нажмите Esc для выхода.\n");
}

// Функция для отображения текущего пути
void displayCurrentPath(const string& currentPath, const vector<string>& bookmarks) {
    COORD coord = { 0, 0 };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

    // Отображение текущего пути
    cout << utf8_to_cp866("Текущий путь: ") << currentPath;

    // Проверка на закладку
    for (size_t i = 0; i < bookmarks.size(); ++i) {
        if (bookmarks[i] == currentPath) {
            cout << utf8_to_cp866(" [Закладка ") << (i + 1) << "]";
            break;
        }
    }

    // Дополнение строки пробелами
    cout << string(80 - currentPath.length() - 15, ' ');
}

// Функция для удаления выбранных файлов
void deleteSelectedFiles(const string& currentPath, vector<FileInfo>& currentFiles) {
    for (auto& file : currentFiles) {
        if (file.isSelected) {
            string filePath = currentPath + "\\" + file.name;
            cout << utf8_to_cp866("Вы уверены, что хотите удалить " + filePath + "? (y/n): ");
            char confirm;
            cin >> confirm;
            if (confirm == 'y' || confirm == 'Y') {
                try {
                    if (file.isDirectory) {
                        fs::remove_all(filePath);
                    } else {
                        fs::remove(filePath);
                    }
                    cout << utf8_to_cp866("Удалено: " + filePath + "\n");
                } catch (const fs::filesystem_error& e) {
                    cout << utf8_to_cp866("Ошибка удаления: " + string(e.what()) + "\n");
                }
            } else {
                cout << utf8_to_cp866("Удаление отменено.\n");
            }
        }
    }
}

// Функция для копирования выбранных файлов
void copySelectedFiles(const string& currentPath, const vector<FileInfo>& currentFiles, const string& destinationPath) {
    for (const auto& file : currentFiles) {
        if (file.isSelected) {
            string sourcePath = currentPath + "\\" + file.name;
            string destPath = destinationPath + "\\" + file.name;

            if (fs::exists(destPath)) {
                cout << utf8_to_cp866("Файл " + destPath + " уже существует. Выберите действие:\n");
                cout << utf8_to_cp866("1. Отмена\n");
                cout << utf8_to_cp866("2. Заменить\n");
                cout << utf8_to_cp866("3. Пропустить\n");
                char choice;
                cin >> choice;
                if (choice == '1') {
                    continue;
                } else if (choice == '3') {
                    continue;
                }
            }

            try {
                if (file.isDirectory) {
                    fs::copy(sourcePath, destPath, fs::copy_options::recursive);
                } else {
                    fs::copy(sourcePath, destPath);
                }
                cout << utf8_to_cp866("Скопировано: " + sourcePath + " -> " + destPath + "\n");
            } catch (const fs::filesystem_error& e) {
                cout << utf8_to_cp866("Ошибка копирования: " + string(e.what()) + "\n");
            }
        }
    }
}

// Функция для перемещения выбранных файлов
void moveSelectedFiles(const string& currentPath, vector<FileInfo>& currentFiles, const string& destinationPath) {
    for (auto& file : currentFiles) {
        if (file.isSelected) {
            string sourcePath = currentPath + "\\" + file.name;
            string destPath = destinationPath + "\\" + file.name;

            if (fs::exists(destPath)) {
                cout << utf8_to_cp866("Файл " + destPath + " уже существует. Выберите действие:\n");
                cout << utf8_to_cp866("1. Отмена\n");
                cout << utf8_to_cp866("2. Заменить\n");
                cout << utf8_to_cp866("3. Пропустить\n");
                char choice;
                cin >> choice;
                if (choice == '1') {
                    continue;
                } else if (choice == '3') {
                    continue;
                }
            }

            try {
                fs::rename(sourcePath, destPath);
                cout << utf8_to_cp866("Перемещено: " + sourcePath + " -> " + destPath + "\n");
            } catch (const fs::filesystem_error& e) {
                cout << utf8_to_cp866("Ошибка перемещения: " + string(e.what()) + "\n");
            }
        }
    }
}

// Функция для переименования файла или директории
void renameFileOrDirectory(const string& currentPath, FileInfo& fileInfo) {
    string oldPath = currentPath + "\\" + fileInfo.name;
    string newName;
    cout << utf8_to_cp866("Введите новое имя (оставьте пустым для отмены): ");
    cin.ignore();
    getline(cin, newName);

    if (newName.empty()) {
        cout << utf8_to_cp866("Переименование отменено.\n");
        return;
    }

    string newPath = currentPath + "\\" + newName;
    try {
        fs::rename(oldPath, newPath);
        fileInfo.name = newName;
        cout << utf8_to_cp866("Переименовано: " + oldPath + " -> " + newPath + "\n");
    } catch (const fs::filesystem_error& e) {
        cout << utf8_to_cp866("Ошибка переименования: " + string(e.what()) + "\n");
    }
}

// Функция для преобразования строки из CP866 в wstring
std::wstring cp866_to_wstring(const std::string& str) {
    if (str.empty()) return L"";

    // Получаем размер необходимого буфера для Unicode строки
    int wchars_num = MultiByteToWideChar(866, 0, str.c_str(), -1, NULL, 0);

    // Создаем буфер для Unicode строки
    std::wstring wstr(wchars_num, L'\0');

    // Конвертируем из CP866 в Unicode (UTF-16)
    MultiByteToWideChar(866, 0, str.c_str(), -1, &wstr[0], wchars_num);

    // Удаляем лишний нулевой символ, если он есть
    if (!wstr.empty() && wstr.back() == L'\0') {
        wstr.pop_back();
    }

    return wstr;
}

// Функция для расчета размера директории
uintmax_t calculateDirectorySize(const std::string& path) {
    uintmax_t totalSize = 0;

    // Конвертируем путь из CP866 в std::wstring (UTF-16)
    std::wstring wpath = cp866_to_wstring(path);

    // Создаем fs::path из std::wstring
    fs::path dirPath(wpath);

    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (fs::is_regular_file(entry)) {
                totalSize += fs::file_size(entry);
            }
        }
    } catch (const fs::filesystem_error& e) {
        // Обработка ошибок
        std::cerr << "Ошибка: " << e.what() << std::endl;
    }

    return totalSize;
}

// Функция для отображения полной информации о файле
void displayFullFileInfo(const FileInfo& fileInfo, const string& currentPath) {
    cout << utf8_to_cp866("Полная информация о файле:\n");
    cout << utf8_to_cp866("Имя: ") << fileInfo.name << "\n";
    cout << utf8_to_cp866("Тип: ") << (fileInfo.isDirectory ? utf8_to_cp866("Директория") : utf8_to_cp866("Файл")) << "\n";

    if (fileInfo.isDirectory) {
        string dirPath = currentPath + "\\" + fileInfo.name;
        uintmax_t dirSize = 0;
        try {
            dirSize = calculateDirectorySize(dirPath);
        } catch (const fs::filesystem_error& e) {
            cout << utf8_to_cp866(string("Ошибка расчета размера директории: ") + e.what() + "\n");
        }
        cout << utf8_to_cp866(string("Размер: ") + to_string(dirSize) + " байт\n");
    } else {
        cout << utf8_to_cp866(string("Размер: ") + to_string(fileInfo.fileData.nFileSizeLow) + " байт\n");
    }

    FILETIME creationTime = fileInfo.fileData.ftCreationTime;
    SYSTEMTIME sysTime;
    FileTimeToSystemTime(&creationTime, &sysTime);
    cout << utf8_to_cp866(string("Дата создания: ") + to_string(sysTime.wDay) + "." + to_string(sysTime.wMonth) + "." + to_string(sysTime.wYear) + "\n");

    FILETIME lastWriteTime = fileInfo.fileData.ftLastWriteTime;
    FileTimeToSystemTime(&lastWriteTime, &sysTime);
    cout << utf8_to_cp866(string("Дата изменения: ") + to_string(sysTime.wDay) + "." + to_string(sysTime.wMonth) + "." + to_string(sysTime.wYear) + "\n");

    cout << utf8_to_cp866(string("Атрибуты: "));
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) cout << utf8_to_cp866(string("Только для чтения "));
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) cout << utf8_to_cp866(string("Скрытый "));
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) cout << utf8_to_cp866(string("Системный "));
    if (fileInfo.fileData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) cout << utf8_to_cp866(string("Архивный "));
    cout << "\n";
}

// Функция для отображения подсказки по клавишам в режиме файлового менеджера
void displayFileKeyHelp() {
    COORD coord = { 0, 24 };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    setColor(8);
    cout << utf8_to_cp866("Стрелки: Навигация | Enter: Открыть | Пробел: Выбрать | B: Открыть в блокноте | D: Удалить | C: Копировать | M: Переместить | R: Переименовать | I: Информация | Q: Выход");
}

// Функция для отображения подсказки по клавишам в режиме просмотра файла
void displayFileViewKeyHelp() {
    COORD coord = { 0, 24 };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    setColor(8);
    cout << utf8_to_cp866("Стрелки: Прокрутка | PageUp/PageDown: Прокрутка на 20 строк | Home/End: Переход к началу/концу | G: Перейти к строке | Esc: Выход");
}

// Основная функция программы
// Функция для вывода справки
void displayHelp() {
    system("cls"); // Очистка экрана
    cout << utf8_to_cp866("Справка по клавишам:\n");
    cout << utf8_to_cp866("----------------------------------------\n");
    cout << utf8_to_cp866("Стрелки: Навигация по файлам и директориям\n");
    cout << utf8_to_cp866("Enter: Открыть директорию или файл в проводнике\n");
    cout << utf8_to_cp866("Shift+Enter: Запустить исполняемый файл в командной строке\n");
    cout << utf8_to_cp866("Пробел: Выбрать/снять выделение с файла\n");
    cout << utf8_to_cp866("B: Открыть файл в блокноте\n");
    cout << utf8_to_cp866("T: Открыть командную строку в текущей директории\n");
    cout << utf8_to_cp866("E: Открыть проводник в текущей директории\n");
    cout << utf8_to_cp866("H: Вывести эту справку\n");
    cout << utf8_to_cp866("S: Сохранить текущий путь как закладку\n");
    cout << utf8_to_cp866("Z: Перейти к закладке\n");
    cout << utf8_to_cp866("D: Удалить выбранные файлы\n");
    cout << utf8_to_cp866("C: Копировать выбранные файлы\n");
    cout << utf8_to_cp866("M: Переместить выбранные файлы\n");
    cout << utf8_to_cp866("R: Переименовать файл или директорию\n");
    cout << utf8_to_cp866("I: Показать полную информацию о файле или директории\n");
    cout << utf8_to_cp866("G: Перейти к строке в режиме просмотра файла\n");
    cout << utf8_to_cp866("Esc: Сбросить выделение файлов или выйти\n");
    cout << utf8_to_cp866("Q: Выйти из программы\n");
    cout << utf8_to_cp866("----------------------------------------\n");
    cout << utf8_to_cp866("Нажмите любую клавишу для продолжения...\n");
    _getch(); // Ожидание нажатия любой клавиши
}

// Объявление функции resetSelectedFiles
//void resetSelectedFiles(vector<FileInfo>& currentFiles, unordered_map<string, bool>& selectionCache, const string& currentPath);

// Определение функции resetSelectedFiles
void resetSelectedFiles(vector<FileInfo>& currentFiles, unordered_map<string, bool>& selectionCache, const string& currentPath) {
    for (auto& file : currentFiles) {
        file.isSelected = false;
        string filePath = currentPath + "\\" + file.name;
        selectionCache[filePath] = false;
    }
}

// Функция для создания нового каталога и установки его как текущего пути
void createAndSetNewDirectory(string& currentPath) {
    string newDirName;
    cout << utf8_to_cp866("Введите имя нового каталога: ");
    cin.ignore(); // Игнорируем предыдущий ввод
    getline(cin, newDirName);

    if (newDirName.empty()) {
        cout << utf8_to_cp866("Имя каталога не может быть пустым.\n");
        return;
    }

    string newDirPath = currentPath + "\\" + newDirName;

    // Создаем новый каталог
    if (CreateDirectoryW(cp866_to_wstring(newDirPath).c_str(), NULL)) {
        cout << utf8_to_cp866("Каталог успешно создан: " + newDirPath + "\n");
        currentPath = newDirPath; // Устанавливаем новый каталог как текущий путь
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            cout << utf8_to_cp866("Каталог уже существует: " + newDirPath + "\n");
        } else {
            cout << utf8_to_cp866("Ошибка при создании каталога: " + to_string(error) + "\n");
        }
    }
}

int main() {
    setConsoleToCP866(); // Устанавливаем кодировку консоли на CP866

    wchar_t buffer[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, buffer);
    string currentPath = utf16_to_cp866(buffer);

    vector<FileInfo> currentFiles, parentFiles;
    int cursorPos = 0;
    int scrollOffset = 0;
    int fileScrollOffset = 0;
    int consoleWidth = 80;
    int consoleHeight = 24;
    bool fullScreenFileView = false;
    bool needClearScreen = true;
    string lastSelectedDir;
    string searchText;

    vector<string> bookmarks(9, "");

    while (true) {
        if (needClearScreen) {
            system("cls");
            needClearScreen = false;

            displayCurrentPath(currentPath, bookmarks);

            if (currentPath.empty()) {
                currentFiles = getDrives();
                parentFiles.clear();
            } else {
                vector<FileInfo> oldFiles = currentFiles;
                vector<FileInfo> newFiles = getFilesInDirectory(currentPath);

                for (auto& newFile : newFiles) {
                    string filePath = currentPath + "\\" + newFile.name;
                    if (selectionCache.find(filePath) != selectionCache.end()) {
                        newFile.isSelected = selectionCache[filePath];
                    }
                }

                currentFiles = newFiles;

                string parentPath = getParentDirectory(currentPath);
                parentFiles = parentPath.empty() ? vector<FileInfo>() : getFilesInDirectory(parentPath);
            }

            if (!fullScreenFileView) {
                displayColumn(parentFiles, 0, 1, consoleWidth / 3, consoleHeight - 1, -1, 0, searchText);
                displayColumn(currentFiles, consoleWidth / 3, 1, consoleWidth / 3, consoleHeight - 1, cursorPos, scrollOffset, searchText);

                if (!currentFiles.empty()) {
                    string selectedPath = currentPath.empty() ? currentFiles[cursorPos].name : currentPath + "\\" + currentFiles[cursorPos].name;
                    if (currentFiles[cursorPos].isDirectory) {
                    } else if (find(sourceFileExtensions.begin(), sourceFileExtensions.end(), currentFiles[cursorPos].extension) != sourceFileExtensions.end()) {
                        displayFileContent(selectedPath, 2 * consoleWidth / 3, 1, consoleWidth / 3, consoleHeight - 1, 0, fullScreenFileView);
                    } else {
                        displayFileInfo(currentFiles[cursorPos], 2 * consoleWidth / 3, 1, consoleWidth / 3, consoleHeight - 1);
                    }
                }
                displayFileKeyHelp();
            } else {
                if (!currentFiles.empty() && !currentFiles[cursorPos].isDirectory) {
                    string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                    displayFileContent(selectedPath, 0, 1, consoleWidth, consoleHeight - 1, fileScrollOffset, fullScreenFileView);
                }
                displayFileViewKeyHelp();
            }
        }

        if (_kbhit()) {
            int key = _getch();

            if (key == 224) {
                key = _getch();

                switch (key) {
                    case 72: // Стрелка вверх
                        if (fullScreenFileView) {
                            if (fileScrollOffset > 0) {
                                fileScrollOffset--;
                                needClearScreen = true;
                            }
                        } else {
                            if (cursorPos > 0) {
                                cursorPos--;
                                if (cursorPos < scrollOffset) {
                                    scrollOffset = cursorPos;
                                }
                                needClearScreen = true;
                            }
                        }
                        break;
                    case 80: // Стрелка вниз
                        if (fullScreenFileView) {
                            int totalLines = countLinesInFile(currentPath + "\\" + currentFiles[cursorPos].name);
                            if (fileScrollOffset < totalLines - consoleHeight + 1) {
                                fileScrollOffset++;
                                needClearScreen = true;
                            }
                        } else {
                            if (cursorPos < currentFiles.size() - 1) {
                                cursorPos++;
                                if (cursorPos >= scrollOffset + consoleHeight - 1) {
                                    scrollOffset = cursorPos - (consoleHeight - 1) + 1;
                                }
                                needClearScreen = true;
                            }
                        }
                        break;
                    case 73: // PageUp
                        if (fullScreenFileView) {
                            fileScrollOffset = max(0, fileScrollOffset - 20);
                            needClearScreen = true;
                        } else {
                            cursorPos = max(0, cursorPos - 20);
                            if (cursorPos < scrollOffset) {
                                scrollOffset = cursorPos;
                            }
                            needClearScreen = true;
                        }
                        break;
                    case 81: // PageDown
                        if (fullScreenFileView) {
                            int totalLines = countLinesInFile(currentPath + "\\" + currentFiles[cursorPos].name);
                            fileScrollOffset = min(fileScrollOffset + 20, max(0, totalLines - consoleHeight + 1));
                            needClearScreen = true;
                        } else {
                            cursorPos = min(static_cast<int>(currentFiles.size()) - 1, cursorPos + 20);
                            if (cursorPos >= scrollOffset + consoleHeight - 1) {
                                scrollOffset = cursorPos - (consoleHeight - 1) + 1;
                            }
                            needClearScreen = true;
                        }
                        break;
                    case 71: // Home
                        if (fullScreenFileView) {
                            fileScrollOffset = 0;
                            needClearScreen = true;
                        } else {
                            cursorPos = 0;
                            scrollOffset = 0;
                            needClearScreen = true;
                        }
                        break;
                    case 79: // End
                        if (fullScreenFileView) {
                            int totalLines = countLinesInFile(currentPath + "\\" + currentFiles[cursorPos].name);
                            fileScrollOffset = max(0, totalLines - consoleHeight + 1);
                            needClearScreen = true;
                        } else {
                            cursorPos = currentFiles.size() - 1;
                            scrollOffset = max(0, static_cast<int>(currentFiles.size()) - consoleHeight + 1);
                            needClearScreen = true;
                        }
                        break;
                    case 75: // Стрелка влево
                        if (fullScreenFileView) {
                            fullScreenFileView = false;
                            needClearScreen = true;
                        } else {
                            string parentPath = getParentDirectory(currentPath);
                            if (!parentPath.empty()) {
                                lastSelectedDir = currentPath.substr(currentPath.find_last_of("\\") + 1);
                                currentPath = parentPath;
                                cursorPos = 0;
                                scrollOffset = 0;
                                searchText.clear();
                                currentFiles = getFilesInDirectory(currentPath);
                                for (size_t i = 0; i < currentFiles.size(); ++i) {
                                    if (currentFiles[i].name == lastSelectedDir) {
                                        cursorPos = i;
                                        break;
                                    }
                                }
                                needClearScreen = true;
                            } else {
                                currentPath = "";
                                cursorPos = 0;
                                scrollOffset = 0;
                                searchText.clear();
                                needClearScreen = true;
                            }
                        }
                        break;
                    case 77: // Стрелка вправо
                        if (!currentFiles.empty()) {
                            if (currentFiles[cursorPos].isDirectory) {
                                currentPath = currentPath.empty() ? currentFiles[cursorPos].name : currentPath + "\\" + currentFiles[cursorPos].name;
                                cursorPos = 0;
                                scrollOffset = 0;
                                searchText.clear();
                                needClearScreen = true;
                            } else if (find(executableExtensions.begin(), executableExtensions.end(), currentFiles[cursorPos].extension) != executableExtensions.end()) {
                                string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                                launchExecutable(selectedPath);
                            } else if (find(sourceFileExtensions.begin(), sourceFileExtensions.end(), currentFiles[cursorPos].extension) != sourceFileExtensions.end()) {
                                fullScreenFileView = !fullScreenFileView;
                                fileScrollOffset = 0;
                                needClearScreen = true;
                            }
                        }
                        break;
                }
            } else if (key == 27) { // Esc
                if (!searchText.empty()) {
                    searchText.clear();
                    needClearScreen = true;
                } else if (fullScreenFileView) {
                    fullScreenFileView = false;
                    needClearScreen = true;
                } else {
                    // Сброс выбранных файлов
                    resetSelectedFiles(currentFiles, selectionCache, currentPath);
                    needClearScreen = true;
                }
            } else if (key == 13) { // Enter
                if (!currentFiles.empty()) {
                    if (currentFiles[cursorPos].isDirectory) {
                        currentPath = currentPath.empty() ? currentFiles[cursorPos].name : currentPath + "\\" + currentFiles[cursorPos].name;
                        cursorPos = 0;
                        scrollOffset = 0;
                        searchText.clear();
                        needClearScreen = true;
                    } else {
                        // Открываем файл в проводнике
                        string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                        ShellExecute(NULL, "open", "explorer.exe", ("/select," + selectedPath).c_str(), NULL, SW_SHOWNORMAL);
                    }
                }
            } else if (key == 84) { // T
                openCmdInCurrentDirectory(currentPath);
            } else if (key == 69) { // E
                ShellExecute(NULL, "open", "explorer.exe", currentPath.c_str(), NULL, SW_SHOWNORMAL);
            } else if (key == 72) { // H
                displayHelp();
                needClearScreen = true;
            } else if (key == 13 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) { // Shift+Enter
                if (!currentFiles.empty() && !currentFiles[cursorPos].isDirectory) {
                    string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                    if (find(executableExtensions.begin(), executableExtensions.end(), currentFiles[cursorPos].extension) != executableExtensions.end()) {
                        string command = "cmd /K \"" + selectedPath + "\"";
                        system(command.c_str());
                    }
                }
            } else if (key == 66) { // B
                if (!currentFiles.empty() && !currentFiles[cursorPos].isDirectory) {
                    string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                    openFileInNotepad(selectedPath);
                }
            } else if (key == 86) { // V (Переключение режима просмотра файла)
                if (!currentFiles.empty() && !currentFiles[cursorPos].isDirectory) {
                    fullScreenFileView = !fullScreenFileView;
                    fileScrollOffset = 0;
                    needClearScreen = true;
                }
            } else if (key == 83) { // S
                displayBookmarks(bookmarks, true);
                while (true) {
                    if (_kbhit()) {
                        int bookmarkKey = _getch();
                        if (bookmarkKey == 27) {
                            needClearScreen = true;
                            break;
                        } else if (bookmarkKey >= '1' && bookmarkKey <= '9') {
                            int index = bookmarkKey - '1';
                            bookmarks[index] = currentPath;
                            needClearScreen = true;
                            break;
                        }
                    }
                }
            } else if (key == 90) { // Z
                displayBookmarks(bookmarks, false);
                while (true) {
                    if (_kbhit()) {
                        int bookmarkKey = _getch();
                        if (bookmarkKey == 27) {
                            needClearScreen = true;
                            break;
                        } else if (bookmarkKey >= '1' && bookmarkKey <= '9') {
                            int index = bookmarkKey - '1';
                            if (!bookmarks[index].empty()) {
                                currentPath = bookmarks[index];
                                cursorPos = 0;
                                scrollOffset = 0;
                                searchText.clear();
                                needClearScreen = true;
                                break;
                            }
                        }
                    }
                }
            } else if (key == 32) { // Пробел
                if (!currentFiles.empty()) {
                    currentFiles[cursorPos].isSelected = !currentFiles[cursorPos].isSelected;
                    string filePath = currentPath + "\\" + currentFiles[cursorPos].name;
                    selectionCache[filePath] = currentFiles[cursorPos].isSelected;

                    if (cursorPos < currentFiles.size() - 1) {
                        cursorPos++;
                        if (cursorPos >= scrollOffset + consoleHeight - 1) {
                            scrollOffset = cursorPos - (consoleHeight - 1) + 1;
                        }
                    }
                    needClearScreen = true;
                }
            } else if (key == 43) { // +
                for (auto& file : currentFiles) {
                    file.isSelected = true;
                    string filePath = currentPath + "\\" + file.name;
                    selectionCache[filePath] = true;
                }
                needClearScreen = true;
            } else if (key == 45) { // -
                for (auto& file : currentFiles) {
                    file.isSelected = false;
                    string filePath = currentPath + "\\" + file.name;
                    selectionCache[filePath] = false;
                }
                needClearScreen = true;
            } else if (key == 42) { // *
                for (auto& file : currentFiles) {
                    file.isSelected = !file.isSelected;
                    string filePath = currentPath + "\\" + file.name;
                    selectionCache[filePath] = file.isSelected;
                }
                needClearScreen = true;
            } else if (key == 68) { // D
                deleteSelectedFiles(currentPath, currentFiles);
                needClearScreen = true;
            } else if (key == 67) { // C
                displayBookmarks(bookmarks, false);
                while (true) {
                    if (_kbhit()) {
                        int bookmarkKey = _getch();
                        if (bookmarkKey == 27) {
                            needClearScreen = true;
                            break;
                        } else if (bookmarkKey >= '1' && bookmarkKey <= '9') {
                            int index = bookmarkKey - '1';
                            if (!bookmarks[index].empty()) {
                                copySelectedFiles(currentPath, currentFiles, bookmarks[index]);
                                needClearScreen = true;
                                break;
                            }
                        }
                    }
                }
            } else if (key == 77) { // M
                displayBookmarks(bookmarks, false);
                while (true) {
                    if (_kbhit()) {
                        int bookmarkKey = _getch();
                        if (bookmarkKey == 27) {
                            needClearScreen = true;
                            break;
                        } else if (bookmarkKey >= '1' && bookmarkKey <= '9') {
                            int index = bookmarkKey - '1';
                            if (!bookmarks[index].empty()) {
                                moveSelectedFiles(currentPath, currentFiles, bookmarks[index]);
                                needClearScreen = true;
                                break;
                            }
                        }
                    }
                }
            } else if (key == 82) { // R
                if (!currentFiles.empty()) {
                    renameFileOrDirectory(currentPath, currentFiles[cursorPos]);
                    needClearScreen = true;
                }
            } else if (key == 73) { // I (Отображение информации о файле или директории)
                if (!currentFiles.empty()) {
                    displayFullFileInfo(currentFiles[cursorPos], currentPath);
                    cout << utf8_to_cp866("Нажмите любую клавишу для продолжения...");
                    _getch();
                    needClearScreen = true;
                }
            } else if (key == 71) { // G (Переход к строке)
                if (fullScreenFileView) {
                    if (!currentFiles.empty() && !currentFiles[cursorPos].isDirectory) {
                        string selectedPath = currentPath + "\\" + currentFiles[cursorPos].name;
                        int totalLines = countLinesInFile(selectedPath);
                        cout << utf8_to_cp866("Введите номер строки для перехода: ");
                        int lineNumber;
                        cin >> lineNumber;
                        fileScrollOffset = max(0, min(lineNumber - 1, totalLines - consoleHeight + 1));
                        needClearScreen = true;
                    }
                }
            } else if (key == 75) { // K (Создание нового каталога)
                createAndSetNewDirectory(currentPath);
                needClearScreen = true;
            } else if (key == 81) { // Q (Выход из программы)
                cout << utf8_to_cp866("Выход из программы...\n");
                return 0; // Завершение программы
            } else if (isalpha(key) || isdigit(key)) {
                searchText += static_cast<char>(tolower(key));

                for (size_t i = 0; i < currentFiles.size(); ++i) {
                    string fileName = currentFiles[i].name;
                    transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

                    if (fileName.find(searchText) == 0) {
                        cursorPos = i;
                        if (cursorPos < scrollOffset) {
                            scrollOffset = cursorPos;
                        } else if (cursorPos >= scrollOffset + consoleHeight - 1) {
                            scrollOffset = cursorPos - (consoleHeight - 1) + 1;
                        }
                        needClearScreen = true;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}


