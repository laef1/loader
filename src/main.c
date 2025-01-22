// main.c
#define _CRT_SECURE_NO_WARNINGS
// main.c
#include <stdio.h>
#include <windows.h>
#include "window_handle.h"

// Forward declarations from file.c
void processNVIDIADownloads(void);
void processAMDDownloads(void);


int main() {
    int loaderchoice;

    // Enable ANSI escape sequences for colors
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }

    ResizeConsole(800, 800);

    while (1) {
        printf("\033[36m");
        // BANNNER
        printf(" /$$$$$$$$ /$$$$$$$$ /$$   /$$ /$$$$$$$$ \n");
        printf("| $$_____/| $$_____/| $$$ | $$|__  $$__/ \n");
        printf("| $$      | $$      | $$$$| $$   | $$ \n");
        printf("| $$$$$   | $$$$$   | $$ $$ $$   | $$ \n");
        printf("| $$__/   | $$__/   | $$  $$$$   | $$ \n");
        printf("| $$      | $$      | $$\\  $$$   | $$ \n");
        printf("| $$      | $$$$$$$$| $$ \\  $$   | $$ \n");
        printf("|__/      |________/|__/  \\__/   |__/ \n");
      
        printf("\n                                 NVIDIA GPU | 1");
        printf("\n                                    AMD GPU | 2\n");
        printf("\033[33m"); // Yellow for instructions
        printf("                                    PICK 1 OR 2\n");
        printf("\n\033[36m"); // Cyan for prompt

        printf("                                   $: ");
        scanf("%d", &loaderchoice);
        if (loaderchoice == 1) {
            system("cls");
            printf("\033[32m[+] Selected NVIDIA GPU\033[0m\n");
            processNVIDIADownloads();
            break;
        }
        else if (loaderchoice == 2) {
            system("cls");
            printf("\033[32m[+] Selected AMD GPU\033[0m\n");
            processAMDDownloads();
            break;
        }
        else {
            system("cls");
            printf("\033[31m[-] Invalid selection. Try again.\033[0m\n");
        }
    }

    printf("\033[37mExiting in 3 seconds...\033[0m\n"); // White exit message.
    Sleep(3000);
    printf("\033[0m"); // Reset colors
    return 0;
}
