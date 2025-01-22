#include <windows.h>
#include <stdio.h>

void ResizeConsole(int width, int height) {
    HWND console = GetConsoleWindow();
    if (console == NULL) {
        fprintf(stderr, "[-] Error: Unable to get console window handle.\n");
        return;
    }
    RECT rect;
    if (GetWindowRect(console, &rect)) {
        int x = rect.left;
        int y = rect.top;
        SetWindowPos(console, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}
