//========================================================================
// GLFWDIAG - A diagnostic tool for GLFW
//------------------------------------------------------------------------
// Copyright (c) 2013 elmindreda <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#ifndef UNICODE
 #define UNICODE
#endif

#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <stdio.h>

#include "diag.h"
#include "resource.h"

#define MAIN_WCL_NAME L"GLFWDIAG"

static struct
{
    HINSTANCE instance;
    HWND window;
    HWND edit;
} state;

static void error(void)
{
    exit(EXIT_FAILURE);
}

static WCHAR* utf16_from_utf8(const char* source)
{
    WCHAR* target;
    int length;

    length = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    if (!length)
        return NULL;

    target = malloc(sizeof(WCHAR) * (length + 1));

    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, target, length + 1))
    {
        free(target);
        return NULL;
    }

    return target;
}

static char* utf8_from_utf16(const WCHAR* source)
{
    char* target;
    int length;

    length = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!length)
        return NULL;

    target = malloc(length + 1);

    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, length + 1, NULL, NULL))
    {
        free(target);
        return NULL;
    }

    return target;
}

static char* get_window_text_utf8(HWND window)
{
    char* text = NULL;
    WCHAR* wideText;
    size_t wideLength;

    wideLength = GetWindowTextLength(window);
    wideText = calloc(wideLength + 1, sizeof(WCHAR));

    if (GetWindowText(window, wideText, wideLength + 1))
        text = utf8_from_utf16(wideText);

    free(wideText);
    return text;
}

static void update_report(void)
{
    WCHAR* report;

    report = utf16_from_utf8(get_report());
    if (!report)
        error();

    SetWindowText(state.edit, report);
    free(report);
}

static void handle_menu_command(int command)
{
    switch (command)
    {
        case IDM_SAVEAS:
        {
            WCHAR path[MAX_PATH + 1];

            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = state.window;
            ofn.hInstance = state.instance;
            ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0"
                              L"All Files (*.*)\0*.*\0"
                              L"\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = path;
            ofn.nMaxFile = sizeof(path) / sizeof(WCHAR);
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = L"txt";

            wcscpy(path, L"GLFWDIAG.txt");

            if (GetSaveFileName(&ofn))
            {
                char* text = get_window_text_utf8(state.edit);
                if (text)
                {
                    FILE* file = _wfopen(path, L"wb");
                    if (file)
                    {
                        fwrite(text, 1, strlen(text), file);
                        fclose(file);
                    }

                    free(text);
                }
            }

            break;
        }

        case IDM_COPY:
        {
            SendMessage(state.edit, WM_COPY, 0, 0);
            break;
        }

        case IDM_SELECTALL:
        {
            Edit_SetSel(state.edit, 0, -1);
            break;
        }

        case IDM_DEFAULTWINDOW:
        {
            ShowWindow(state.window, SW_HIDE);

            test_default_window();
            update_report();

            ShowWindow(state.window, SW_SHOWNORMAL);
            break;
        }

        case IDM_EXIT:
        {
            DestroyWindow(state.window);
            break;
        }
    }
}

static LRESULT CALLBACK main_window_proc(HWND window,
                                         UINT message,
                                         WPARAM wParam,
                                         LPARAM lParam)
{
    switch (message)
    {
        case WM_COMMAND:
        {
            handle_menu_command(LOWORD(wParam));
            return 0;
        }

        case WM_SIZE:
        {
            MoveWindow(state.edit, 0, 0, LOWORD(lParam), HIWORD(lParam), FALSE);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(window, message, wParam, lParam);
}

static int register_main_class(void)
{
    WNDCLASS wcl;
    ZeroMemory(&wcl, sizeof(wcl));

    wcl.style = CS_HREDRAW | CS_VREDRAW;
    wcl.lpfnWndProc = main_window_proc;
    wcl.hInstance = state.instance;
    wcl.hIcon = LoadIcon(state.instance, MAKEINTRESOURCE(IDI_GLFWDIAG));
    wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcl.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wcl.lpszMenuName = MAKEINTRESOURCE(IDC_MAIN);
    wcl.lpszClassName = MAIN_WCL_NAME;

    return RegisterClass(&wcl);
}

static int create_main_window(int show)
{
    RECT client;
    HFONT font;

    state.window = CreateWindowEx(WS_EX_APPWINDOW,
                                  MAIN_WCL_NAME,
                                  L"GLFW Diagnostics Tool",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, 0,
                                  CW_USEDEFAULT, 0,
                                  NULL, NULL, state.instance, NULL);
    if (!state.window)
        return FALSE;

    GetClientRect(state.window, &client);

    state.edit = CreateWindowEx(WS_EX_CLIENTEDGE,
                                L"EDIT",
                                L"",
                                WS_VISIBLE | WS_HSCROLL |
                                    WS_VSCROLL | WS_CHILD |
                                    ES_AUTOVSCROLL | ES_MULTILINE |
                                    ES_READONLY | ES_NOHIDESEL,
                                0, 0, client.right, client.bottom,
                                state.window, NULL, state.instance, NULL);
    if (!state.edit)
        return FALSE;

    font = CreateFont(0, 0, 0, 0, FW_NORMAL,
                      FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY,
                      DEFAULT_PITCH,
                      L"Courier New");
    if (!font)
        return FALSE;

    SetWindowFont(state.edit, font, FALSE);
    Edit_LimitText(state.edit, 0);

    ShowWindow(state.window, show);
    UpdateWindow(state.window);

    return TRUE;
}

int APIENTRY WinMain(HINSTANCE instance,
                     HINSTANCE previous,
                     LPSTR commandLine,
                     int show)
{
    MSG msg;

    UNREFERENCED_PARAMETER(previous);
    UNREFERENCED_PARAMETER(commandLine);

    ZeroMemory(&state, sizeof(state));
    state.instance = instance;

    if (!report_init())
        error();

    if (!register_main_class())
        error();

    if (!create_main_window(show))
        error();

    report_monitors();
    report_joysticks();
    update_report();

    for (;;)
    {
        const BOOL result = GetMessage(&msg, NULL, 0, 0);

        if (result == 0)
            break;

        if (result == -1)
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    report_terminate();
    exit(EXIT_SUCCESS);
}

