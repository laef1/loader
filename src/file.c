#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include <sys/stat.h>
#include <objbase.h>
#include <curl/curl.h>

#pragma comment(lib, "libcurl.lib")  // Ensure libcurl is linked via vcpkg

//------------------------------------------------------------------------------
// Helper: Write callback for libcurl.
static size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream) {
    FILE* fp = (FILE*)stream;
    return fwrite(ptr, size, nmemb, fp);
}

//------------------------------------------------------------------------------
// Progress callback for libcurl to show a filling progress bar.
static int progress_callback(void* clientp, curl_off_t total_to_download, curl_off_t now_downloaded,
    curl_off_t total_to_upload, curl_off_t now_uploaded) {
    (void)clientp;
    (void)total_to_upload;
    (void)now_uploaded;

    int width = 50;  // progress bar width in characters
    double fraction = 0.0;
    if (total_to_download > 0)
        fraction = (double)now_downloaded / (double)total_to_download;
    int filled = (int)(fraction * width);

    // Carriage return to update the same line
    printf("\r[");
    for (int i = 0; i < width; i++) {
        if (i < filled)
            printf("#");
        else
            printf(" ");
    }
    printf("] %3.0f%%", fraction * 100);
    fflush(stdout);

    return 0; // return 0 to continue download
}

//------------------------------------------------------------------------------
// File size in bytes.
long get_file_size(const char* filename) {
    struct _stat st;
    if (_stat(filename, &st) == 0)
        return (long)st.st_size;
    return -1;
}

//------------------------------------------------------------------------------
// Debug: Print first N bytes from file.
void print_file_preview(const char* filename, size_t previewSize) {
    FILE* fp = fopen(filename, "rb");
    if (fp) {
        char* preview = malloc(previewSize + 1);
        if (preview) {
            size_t bytesRead = fread(preview, 1, previewSize, fp);
            preview[bytesRead] = '\0';
            printf("\n[DEBUG] File preview (%zu bytes):\n%s\n", bytesRead, preview);
            free(preview);
        }
        fclose(fp);
    }
}

//------------------------------------------------------------------------------
// Download file using libcurl with progress bar.
// Returns 0 on success, -1 on failure.
int download_file(const char* url, const char* output_filename) {
    CURL* curl;
    CURLcode res;
    FILE* fp = fopen(output_filename, "wb");
    if (!fp) {
        fprintf(stderr, "\033[31m[-] Failed to open output file %s for writing.\033[0m\n", output_filename);
        return -1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url);
        // Set write callback function.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        // Set the file stream as user data.
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        // Set a browser–like user agent.
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
        // Follow redirects.
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // Enable progress meter.
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        // Set progress callback function.
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        // Perform download.
        res = curl_easy_perform(curl);
        // Add a newline after progress bar.
        printf("\n");
        if (res != CURLE_OK) {
            fprintf(stderr, "\033[31m[-] curl_easy_perform() failed: %s\033[0m\n", curl_easy_strerror(res));
            fclose(fp);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return -1;
        }
        curl_easy_cleanup(curl);
    }
    else {
        fclose(fp);
        curl_global_cleanup();
        return -1;
    }
    fclose(fp);
    curl_global_cleanup();
    return 0;
}

//------------------------------------------------------------------------------
// Helper: Check whether a file exists.
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
    const char* winrar_url = "https://www.rarlab.com/rar/winrar-x64-611.exe";
    const char* winrar_installer = "winrar_installer.exe";
    if (download_file(winrar_url, winrar_installer) != 0) {
        fprintf(stderr, "\033[31m[-] Failed to download WinRAR installer.\033[0m\n");
        return -1;
    }
    printf("\033[37m[+] Installing WinRAR silently...\033[0m\n");
    if (system("winrar_installer.exe /s") != 0) {
        fprintf(stderr, "\033[31m[-] WinRAR silent installation failed.\033[0m\n");
        return -1;
    }
    Sleep(10000); // Wait 10 seconds for installation.
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
// Unpack a RAR file using UnRAR.exe into the specified directory.
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
void processNVIDIADownloads(void) {
    const char* dep_url = "SUPPLY LINK";
    const char* exe_url = "SUPPLY LINK"; 
    const char* folder = "NVIDIA";
    const char* dep_file = "NVIDIA\\net8.0-windows_nvidia_dependencies.rar";
    const char* exe_file = "NVIDIA\\net8.0-windows_nvidia_exe.rar";

    if (ensure_unrar_exists() != 0) {
        fprintf(stderr, "\033[31m[-] UnRAR.exe is required for extraction.\033[0m\n");
        return;
    }
    if (create_folder(folder) != 0)
        return;

    printf("\033[33m[!] Downloading NVIDIA dependencies...\033[0m\n");
    if (download_file(dep_url, dep_file) == 0) {
        long depSize = get_file_size(dep_file);
        printf("\033[37m[+] NVIDIA Dependencies downloaded file size: %ld bytes.\033[0m\n", depSize);
        // Check if file appears to be an HTML page.
        FILE* fp = fopen(dep_file, "rb");
        if (fp) {
            char header[32] = { 0 };
            fread(header, 1, sizeof(header) - 1, fp);
            fclose(fp);
            if (strstr(header, "<html") != NULL) {
                fprintf(stderr, "\033[31m[-] NVIDIA Dependencies file appears to be an HTML page. The link may be disabled or invalid.\033[0m\n");
                print_file_preview(dep_file, 512);
                return;
            }
        }
        if (depSize < 1300000000) {
            fprintf(stderr, "\033[31m[-] NVIDIA Dependencies file is too small; download may be incomplete.\033[0m\n");
            print_file_preview(dep_file, 512);
            return;
        }
        unpack_rar(dep_file, folder);
        if (remove(dep_file) == 0)
            printf("\033[37m[+] Removed NVIDIA Dependencies file %s after extraction.\033[0m\n", dep_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove NVIDIA Dependencies file %s.\033[0m\n", dep_file);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download NVIDIA dependencies.\033[0m\n");
        return;
    }

    printf("\033[33m[!] Downloading NVIDIA EXE...\033[0m\n");
    if (download_file(exe_url, exe_file) == 0) {
        long exeSize = get_file_size(exe_file);
        printf("\033[37m[+] NVIDIA EXE downloaded file size: %ld bytes.\033[0m\n", exeSize);
        if (exeSize < 5242880) {  // 5 MB threshold
            fprintf(stderr, "\033[31m[-] NVIDIA EXE file is too small; download may be incomplete.\033[0m\n");
            return;
        }
        unpack_rar(exe_file, folder);
        if (remove(exe_file) == 0)
            printf("\033[37m[+] Removed NVIDIA EXE file %s after extraction.\033[0m\n", exe_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove NVIDIA EXE file %s.\033[0m\n", exe_file);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download NVIDIA EXE.\033[0m\n");
        return;
    }
}

void processAMDDownloads(void) {
    const char* rar_url = "https://cdn.discordapp.com/attachments/1331467559005847603/1331511295287099474/net8.0-windows.rar?ex=6791e207&is=67909087&hm=7d1517a8d00509918b7022d2d0f26bfd6f9435f4a554288cb5b9a83474b34c3a&";
    const char* folder = "AMD";
    const char* rar_file = "AMD\\net8.0-windows_amd.rar";

    if (ensure_unrar_exists() != 0) {
        fprintf(stderr, "\033[31m[-] UnRAR.exe is required for extraction.\033[0m\n");
        return;
    }
    if (create_folder(folder) != 0)
        return;
    printf("\033[33m[!] Downloading AMD file...\033[0m\n");
    if (download_file(rar_url, rar_file) == 0) {
        long amdSize = get_file_size(rar_file);
        printf("\033[37m[+] AMD downloaded file size: %ld bytes.\033[0m\n", amdSize);
        if (amdSize < 19000000) {
            fprintf(stderr, "\033[31m[-] AMD file is too small; download may be incomplete.\033[0m\n");
            print_file_preview(rar_file, 512);
            return;
        }
        unpack_rar(rar_file, folder);
        if (remove(rar_file) == 0)
            printf("\033[37m[+] Removed AMD file %s after extraction.\033[0m\n", rar_file);
        else
            fprintf(stderr, "\033[31m[-] Failed to remove AMD file %s.\033[0m\n", rar_file);
        printf("\033[32m[+] AMD processing complete. Files extracted into folder \"%s\".\033[0m\n", folder);
    }
    else {
        fprintf(stderr, "\033[31m[-] Failed to download AMD RAR file.\033[0m\n");
    }
}
