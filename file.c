#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winhttp.h>
#include <urlmon.h>
#include <string.h>
#include <sys/stat.h>
#include <objbase.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")

//------------------------------------------------------------------------------
// File size in bytes
long get_file_size(const char* filename) {
    struct _stat st;
    if (_stat(filename, &st) == 0)
        return (long)st.st_size;
    return -1;
}

//------------------------------------------------------------------------------
// Download a file using WinHTTP
// This function no longer enforces a minimum file size.
int download_file_winhttp(const char* url, const char* output_filename) {
    DWORD totalDownloaded = 0, bytesRead = 0;
    LPSTR buffer = NULL;
    BOOL bResults = FALSE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    // Convert URL to wide char.
    wchar_t wurl[2048];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, sizeof(wurl) / sizeof(wchar_t));

    // Initialize URL_COMPONENTS.
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;

    if (!WinHttpCrackUrl(wurl, 0, 0, &urlComp)) {
        fprintf(stderr, "\033[31m[-] WinHttpCrackUrl failed with error: %d\033[0m\n", GetLastError());
        return -1;
    }

    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[1024] = { 0 };
    wcsncpy(hostName, urlComp.lpszHostName, urlComp.dwHostNameLength);
    hostName[urlComp.dwHostNameLength] = L'\0';
    wcsncpy(urlPath, urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    urlPath[urlComp.dwUrlPathLength] = L'\0';
    if (urlComp.dwExtraInfoLength > 0)
        wcsncat(urlPath, urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);

    // Open a WinHTTP session using a common browser User-Agent.
    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        fprintf(stderr, "\033[31m[-] WinHttpOpen failed with error: %d\033[0m\n", GetLastError());
        return -1;
    }

    hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        fprintf(stderr, "\033[31m[-] WinHttpConnect failed with error: %d\033[0m\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return -1;
    }

    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        fprintf(stderr, "\033[31m[-] WinHttpOpenRequest failed with error: %d\033[0m\n", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    // Enable automatic redirection.
    DWORD dwRedirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &dwRedirectPolicy, sizeof(dwRedirectPolicy));

    // Add an Accept header.
    LPCWSTR acceptHeader = L"Accept: application/octet-stream";
    WinHttpAddRequestHeaders(hRequest, acceptHeader, -1L, WINHTTP_ADDREQ_FLAG_ADD);

    bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults) {
        fprintf(stderr, "\033[31m[-] WinHttpSendRequest failed with error: %d\033[0m\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        fprintf(stderr, "\033[31m[-] WinHttpReceiveResponse failed with error: %d\033[0m\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    // Get HTTP status code.
    DWORD dwStatusCode = 0;
    DWORD statusSize = sizeof(dwStatusCode);
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        fprintf(stderr, "\033[31m[-] Failed to query HTTP status code. Error: %d\033[0m\n", GetLastError());
    }
    else {
        printf("\033[37m[+] HTTP Status Code: %d\033[0m\n", dwStatusCode);
    }

    // Get the Content-Type header.
    wchar_t contentType[256] = { 0 };
    statusSize = sizeof(contentType);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
        contentType, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        char cType[256];
        WideCharToMultiByte(CP_UTF8, 0, contentType, -1, cType, sizeof(cType), NULL, NULL);
        printf("\033[37m[+] Content-Type: %s\033[0m\n", cType);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to query Content-Type header. Error: %d\033[0m\n", GetLastError());
    }

    // Accept any Content-Type that starts with "application/"
    if (wcsncmp(contentType, L"application/", 12) != 0) {
        fprintf(stderr, "\033[31m[-] Unexpected Content-Type. Expected one starting with 'application/' but got: ");
        char cType[256];
        WideCharToMultiByte(CP_UTF8, 0, contentType, -1, cType, sizeof(cType), NULL, NULL);
        fprintf(stderr, "%s\033[0m\n", cType);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    // Get the Content-Length header.
    DWORD contentLength = 0;
    statusSize = sizeof(contentLength);
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        fprintf(stderr, "\033[31m[-] Failed to query Content-Length header. Error: %d\033[0m\n", GetLastError());
    }
    else {
        printf("\033[37m[+] Content-Length: %lu bytes.\033[0m\n", contentLength);
    }

    // Open the output file.
    FILE* fp = fopen(output_filename, "wb");
    if (!fp) {
        fprintf(stderr, "\033[31m[-] Failed to open output file %s.\033[0m\n", output_filename);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    const DWORD bufSize = 8192;
    buffer = (LPSTR)malloc(bufSize);
    if (!buffer) {
        fprintf(stderr, "\033[31m[-] Out of memory.\033[0m\n");
        fclose(fp);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return -1;
    }

    DWORD totalRead = 0;
    do {
        bytesRead = 0;
        if (!WinHttpReadData(hRequest, (LPVOID)buffer, bufSize, &bytesRead)) {
            fprintf(stderr, "\033[31m[-] WinHttpReadData failed with error: %d\033[0m\n", GetLastError());
            break;
        }
        if (bytesRead == 0)
            break;
        fwrite(buffer, 1, bytesRead, fp);
        totalRead += bytesRead;
        if (contentLength > 0) {
            int percent = (int)((totalRead * 100) / contentLength);
            printf("\r\033[37m[+] Download progress: %3d%%\033[0m", percent);
            fflush(stdout);
        }
    } while (bytesRead > 0);

    free(buffer);
    fclose(fp);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    printf("\r\033[37m[+] Download progress: 100%%\033[0m\n");

    long fileSize = get_file_size(output_filename);
    printf("\033[37m[+] Downloaded file size: %ld bytes.\033[0m\n", fileSize);
    // Do not enforce a universal threshold here; size checks will be done per process.
    return 0;
}

//------------------------------------------------------------------------------
// Helper: Check whether a file exists (used only within file.c).
static int file_exists_local(const char* path) {
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

//------------------------------------------------------------------------------
// Check if UnRAR.exe exists in the current directory.
int check_unrar_exists(void) {
    FILE* file = fopen("UnRAR.exe", "rb");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Ensure UnRAR.exe is available.
int ensure_unrar_exists(void) {
    if (check_unrar_exists()) {
        printf("\033[37m[+] UnRAR.exe found in current directory.\033[0m\n");
        return 0;
    }
    const char* possiblePaths[] = {
        "C:\\Program Files\\WinRAR\\UnRAR.exe",
        "C:\\Program Files (x86)\\WinRAR\\UnRAR.exe"
    };
    int numPaths = sizeof(possiblePaths) / sizeof(possiblePaths[0]);
    for (int i = 0; i < numPaths; i++) {
        if (file_exists_local(possiblePaths[i])) {
            if (CopyFileA(possiblePaths[i], "UnRAR.exe", FALSE)) {
                SetFileAttributesA("UnRAR.exe", FILE_ATTRIBUTE_HIDDEN);
                printf("\033[37m[+] UnRAR.exe copied from \"%s\" to current directory.\033[0m\n", possiblePaths[i]);
                return 0;
            }
            else {
                fprintf(stderr, "\033[31m[-] Failed to copy UnRAR.exe from \"%s\".\033[0m\n", possiblePaths[i]);
            }
        }
    }
    printf("\033[37m[+] UnRAR.exe not found in standard directories. Attempting to download WinRAR...\033[0m\n");
    
    // Replace the WinRAR URL with your own or provide instructions for users to specify it.
    const char* winrar_url = "YOUR_WINRAR_DOWNLOAD_URL_HERE";  // TODO: Replace with your WinRAR download URL.
    const char* winrar_installer = "winrar_installer.exe";
    if (download_file_winhttp(winrar_url, winrar_installer) != 0) {
        fprintf(stderr, "\033[31m[-] Failed to download WinRAR installer.\033[0m\n");
        return -1;
    }
    printf("\033[37m[+] Installing WinRAR silently...\033[0m\n");
    if (system("winrar_installer.exe /s") != 0) {
        fprintf(stderr, "\033[31m[-] WinRAR silent installation failed.\033[0m\n");
        return -1;
    }
    Sleep(10000); // wait 10 seconds for installation
    for (int i = 0; i < numPaths; i++) {
        if (file_exists_local(possiblePaths[i])) {
            if (CopyFileA(possiblePaths[i], "UnRAR.exe", FALSE)) {
                SetFileAttributesA("UnRAR.exe", FILE_ATTRIBUTE_HIDDEN);
                printf("\033[37m[+] UnRAR.exe copied from \"%s\" to current directory after WinRAR installation.\033[0m\n", possiblePaths[i]);
                return 0;
            }
            else {
                fprintf(stderr, "\033[31m[-] Failed to copy UnRAR.exe from \"%s\" after installation.\033[0m\n", possiblePaths[i]);
            }
        }
    }
    fprintf(stderr, "\033[31m[-] Unable to obtain UnRAR.exe.\033[0m\n");
    return -1;
}

//------------------------------------------------------------------------------
// Unpack a RAR file using UnRAR.exe
void unpack_rar(const char* rar_file, const char* output_dir) {
    char command[512];
    size_t len = strlen(output_dir);
    char output_dir_slash[256];

    if (output_dir[len - 1] != '/' && output_dir[len - 1] != '\\') {
        strncpy_s(output_dir_slash, sizeof(output_dir_slash), output_dir, _TRUNCATE);
        strncat_s(output_dir_slash, sizeof(output_dir_slash), "\\", _TRUNCATE);
    }
    else {
        strncpy_s(output_dir_slash, sizeof(output_dir_slash), output_dir, _TRUNCATE);
    }

    snprintf(command, sizeof(command), "UnRAR.exe x -y \"%s\" \"%s\"", rar_file, output_dir_slash);
    printf("\033[37m[+] Extracting contents of %s into folder \"%s\"...\033[0m\n", rar_file, output_dir_slash);
    int ret = system(command);
    if (ret == 0) {
        printf("\033[32m[+] Extraction complete.\033[0m\n");
    }
    else {
        fprintf(stderr, "\033[31m[-] Extraction failed with code %d.\033[0m\n", ret);
    }
}

//------------------------------------------------------------------------------
// Create a folder if it doesn't exist.
int create_folder(const char* folder_name) {
    if (CreateDirectoryA(folder_name, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("\033[37m[+] Folder \"%s\" is ready.\033[0m\n", folder_name);
        return 0;
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to create folder \"%s\".\033[0m\n", folder_name);
        return -1;
    }
}

//------------------------------------------------------------------------------
// Expected thresholds:
// - Dependencies: ≥ 1.3 GiB (~1,300,000,000 bytes)
// - EXE: ≥ 5 MB (~5,242,880 bytes)
void processNVIDIADownloads(void) {
    // Replace these URLs with your own.
    const char* dep_url = "YOUR_NVIDIA_DEPENDENCIES_URL_HERE"; // TODO: Insert your NVIDIA dependencies download URL.
    const char* exe_url = "YOUR_NVIDIA_EXE_URL_HERE";         // TODO: Insert your NVIDIA EXE download URL.
    const char* folder = "NVIDIA";
    const char* dep_file = "NVIDIA\\net8.0-windows_nvidia_dependencies.rar";
    const char* exe_file = "NVIDIA\\net8.0-windows_nvidia_exe.rar";

    if (ensure_unrar_exists() != 0) {
        fprintf(stderr, "\033[31m[-] UnRAR.exe is required for extraction.\033[0m\n");
        return;
    }
    if (create_folder(folder) != 0) return;

    printf("\033[33m[!] Downloading dependencies...\033[0m\n");
    if (download_file_winhttp(dep_url, dep_file) == 0) {
        long depSize = get_file_size(dep_file);
        printf("\033[37m[+] Dependencies downloaded file size: %ld bytes.\033[0m\n", depSize);
        if (depSize < 1300000000) {
            fprintf(stderr, "\033[31m[-] Dependencies file is too small; download may be incomplete.\033[0m\n");
            return;
        }
        unpack_rar(dep_file, folder);
        if (remove(dep_file) == 0)
            printf("\033[37m[+] Removed downloaded file %s after extraction.\033[0m\n", dep_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove downloaded file %s.\033[0m\n", dep_file);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download NVIDIA dependencies.\033[0m\n");
        return;
    }

    printf("\033[33m[!] Downloading EXE...\033[0m\n");
    if (download_file_winhttp(exe_url, exe_file) == 0) {
        long exeSize = get_file_size(exe_file);
        printf("\033[37m[+] EXE downloaded file size: %ld bytes.\033[0m\n", exeSize);
        if (exeSize < 5242880) {  // 5 MB threshold
            fprintf(stderr, "\033[31m[-] EXE file is too small; download may be incomplete.\033[0m\n");
            return;
        }
        unpack_rar(exe_file, folder);
        if (remove(exe_file) == 0)
            printf("\033[37m[+] Removed downloaded file %s after extraction.\033[0m\n", exe_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove downloaded file %s.\033[0m\n", exe_file);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download NVIDIA EXE.\033[0m\n");
        return;
    }
}

//------------------------------------------------------------------------------
// Expected threshold: ≥ 19 MB.
void processAMDDownloads(void) {
    // Replace this URL with your own.
    const char* rar_url = "YOUR_AMD_RAR_URL_HERE"; // TODO: Insert your AMD RAR download URL.
    const char* folder = "AMD";
    const char* rar_file = "AMD\\net8.0-windows_amd.rar";

    if (ensure_unrar_exists() != 0) {
        fprintf(stderr, "\033[31m[-] UnRAR.exe is required for extraction.\033[0m\n");
        return;
    }
    if (create_folder(folder) != 0) return;
    if (download_file_winhttp(rar_url, rar_file) == 0) {
        long amdSize = get_file_size(rar_file);
        printf("\033[37m[+] AMD downloaded file size: %ld bytes.\033[0m\n", amdSize);
        if (amdSize < 19000000) {
            fprintf(stderr, "\033[31m[-] AMD file is too small; download may be incomplete.\033[0m\n");
            return;
        }
        unpack_rar(rar_file, folder);
        if (remove(rar_file) == 0)
            printf("\033[37m[+] Removed downloaded file %s after extraction.\033[0m\n", rar_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove downloaded file %s.\033[0m\n", rar_file);
        printf("\033[32m[+] AMD processing complete. Files extracted into folder \"%s\".\033[0m\n", folder);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download AMD RAR file.\033[0m\n");
    }
}
