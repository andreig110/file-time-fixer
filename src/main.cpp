#include <iostream>
#include <string>
#include <chrono>

#include <io.h>
#include <fcntl.h>
#include <windows.h>

using namespace std;
using namespace std::chrono;

struct ProgramOptions
{
    bool simulate; // Do not change files timestamp. The program will not make any changes to the file system.
} programOptions;

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
        if (programOptions.simulate)
        {
            wcout << filePath << endl;
            if (isFile)
                numFilesFixed++;
            else // if is directory
                numDirsFixed++;
            return;
        }

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
        wcout << "Usage: FileTimeFixer.exe [options] <directory_path>" << endl;
        return 1;
    }

    // Parse command line arguments.
    wstring directoryPath;
    if (argc == 2)
        directoryPath = argv[1];
    else //if (argc > 2)
    {
        // Set program options from command line arguments.
        for (int i = 1; i < argc - 1; ++i)
        {
            if (wcscmp(argv[i], L"--simulate") == 0)
                programOptions.simulate = true;
            else
            {
                wcout << "Unknown option: " << argv[i] << endl;
                return 2;
            }
        }
        directoryPath = argv[argc - 1];
    }

    if (programOptions.simulate)
    {
        wcout << "Simulating..." << endl;
        wcout << "The creation time of the next files should be fixed:" << endl;
    }
    else
        wcout << "Fixing files creation time:" << endl;

    auto start = high_resolution_clock::now();
    ProcessDirectory(directoryPath);
    auto end = high_resolution_clock::now();

    auto tpDuration = (end - start).count();
    double duration;
    wstring unit;
    if (tpDuration < int(1e6))  // microseconds
    {
        duration = tpDuration / 1e3;
        unit = L" \u03BCs";
    }
    else if (tpDuration < int(1e9))  // milliseconds
    {
        duration = tpDuration / 1e6;
        unit = L" ms";
    }
    else  // seconds
    {
        duration = tpDuration / 1e9;
        unit = L" sec";
    }
    duration = int(duration * 10.0) / 10.0;

    // Printing statistics.
    wcout << endl << "Statistics:" << endl;
    wstring was = programOptions.simulate ? L"should be" : L"was";
    wcout << "The number of files for which the creation time " << was << " fixed: " << numFilesFixed << endl;
    wcout << "The number of directories for which the creation time " << was << " fixed: " << numDirsFixed << endl;
    wcout << "Total files processed: " << numFiles << endl;
    wcout << "Total directories processed: " << numDirs << endl;
    wcout << "Execution time: " << duration << unit << endl;

    return 0;
}
