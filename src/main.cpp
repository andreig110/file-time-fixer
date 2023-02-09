#include <iostream>
#include <string>
#include <chrono>

#include <io.h>
#include <fcntl.h>
#include <windows.h>

using namespace std;
using namespace std::chrono;

// Global variables for statistics.
int numFilesFixed;  // The number of files for which the creation time was fixed.
int numDirsFixed;   // The number of directories for which the creation time was fixed.
int numFiles;       // Total files processed.
int numDirs;        // Total directories processed.

void PrintLastError()
{
    wcout << "- Last error: " << GetLastError() << endl;
}

void ProcessFile(const wstring& filePath, bool isFile)
{
    DWORD flagsAndAttributes;
    if (isFile)
    {
        flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        numFiles++;
    }
    else // if is directory
    {
        flagsAndAttributes = FILE_FLAG_BACKUP_SEMANTICS;
        numDirs++;
    }
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, flagsAndAttributes, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        wcout << "Error opening file: " << filePath << endl;
        PrintLastError();
        return;
    }

    FILETIME ftCreation, ftLastWrite;
    BOOL result = GetFileTime(hFile, &ftCreation, NULL, &ftLastWrite);
    CloseHandle(hFile);
    if (!result)
    {
        wcout << "Error getting file times for file: " << filePath << endl;
        PrintLastError();
        return;
    }

    if (CompareFileTime(&ftCreation, &ftLastWrite) > 0)
    {
        hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, flagsAndAttributes, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            wcout << "Error opening file with the GENERIC_WRITE access right: " << filePath << endl;
            PrintLastError();
            return;
        }

        wcout << filePath << " ... ";
        result = SetFileTime(hFile, &ftLastWrite, NULL, &ftLastWrite);
        if (result)
        {
            wcout << "done." << endl;
            if (isFile)
                numFilesFixed++;
            else // if is directory
                numDirsFixed++;
        }
        else
        {
            wcout << "error!" << endl;
            PrintLastError();
        }
        CloseHandle(hFile);
    }
}

void ProcessDirectory(const wstring& directoryPath)
{
    wstring searchPath = directoryPath + L"\\*";
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        wcout << "Error finding files in directory: " << directoryPath << endl;
        PrintLastError();
        return;
    }

    do
    {
        wstring fileName = findFileData.cFileName;
        if (fileName == L".")
            ProcessFile(directoryPath, false);
        if (fileName == L"." || fileName == L"..")
            continue;

        wstring filePath = directoryPath + L"\\" + fileName;
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ProcessDirectory(filePath);
        else
            ProcessFile(filePath, true);
    } while (FindNextFileW(hFind, &findFileData));

    DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
        wcout << "FindNextFileW() error (" << lastError << ") for directory: " << directoryPath << endl;

    FindClose(hFind);
}

int wmain(int argc, wchar_t* argv[])
{
    // Change stdout to Unicode UTF-16 mode.
    _setmode(_fileno(stdout), _O_U16TEXT);

    if (argc < 2)
    {
        wcout << "Usage: FileTimeFixer.exe <directory_path>" << endl;
        return 1;
    }

    wcout << "Fixing files creation time:" << endl;
    wstring directoryPath = argv[1];
    auto start = high_resolution_clock::now();
    ProcessDirectory(directoryPath);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<microseconds>(end - start);
    wstring unit = L" \u03BCs";
    if (duration.count() >= 10000)
    {
        duration = duration_cast<milliseconds>(end - start);
        unit = L" ms";
        if (duration.count() >= 10000)
        {
            duration = duration_cast<seconds>(end - start);
            unit = L" sec";
        }
    }

    // Printing statistics.
    wcout << endl << "Statistics:" << endl;
    wcout << "The number of files for which the creation time was fixed: " << numFilesFixed << endl;
    wcout << "The number of directories for which the creation time was fixed: " << numDirsFixed << endl;
    wcout << "Total files processed: " << numFiles << endl;
    wcout << "Total directories processed: " << numDirs << endl;
    wcout << "Execution time: " << duration.count() << unit << endl;

    return 0;
}
