/*
 * UIAccess Overlay - Fullscreen support for External Cheats
 * Based on: https://yougame.biz/threads/348163/
 * Source: https://github.com/killtimer0/uiaccess
 * 
 * This allows the overlay to work on top of FULLSCREEN games (including 4:3)
 * by using CreateWindowInBand with UIAccess token.
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>

namespace uiaccess {

    // Window band types for CreateWindowInBand
    enum ZBID {
        ZBID_DEFAULT = 0,
        ZBID_DESKTOP = 1,
        ZBID_UIACCESS = 2,          // UIAccess band - above fullscreen apps
        ZBID_IMMERSIVE_IHM = 3,
        ZBID_IMMERSIVE_NOTIFICATION = 4,
        ZBID_IMMERSIVE_APPCHROME = 5,
        ZBID_IMMERSIVE_MOGO = 6,
        ZBID_IMMERSIVE_EDGY = 7,
        ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
        ZBID_IMMERSIVE_INACTIVEDOCK = 9,
        ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
        ZBID_IMMERSIVE_ACTIVEDOCK = 11,
        ZBID_IMMERSIVE_BACKGROUND = 12,
        ZBID_IMMERSIVE_SEARCH = 13,
        ZBID_GENUINE_WINDOWS = 14,
        ZBID_IMMERSIVE_RESTRICTED = 15,
        ZBID_SYSTEM_TOOLS = 16,
        ZBID_LOCK = 17,
        ZBID_ABOVELOCK_UX = 18
    };

    // CreateWindowInBand function pointer type
    typedef HWND(WINAPI* CreateWindowInBand_t)(
        DWORD dwExStyle,
        ATOM classAtom,
        LPCWSTR lpWindowName,
        DWORD dwStyle,
        int X, int Y, int nWidth, int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam,
        DWORD dwBand
    );

    // Get CreateWindowInBand function pointer
    inline CreateWindowInBand_t GetCreateWindowInBand() {
        static CreateWindowInBand_t fn = nullptr;
        if (!fn) {
            fn = (CreateWindowInBand_t)GetProcAddress(
                GetModuleHandleW(L"user32.dll"), 
                "CreateWindowInBand"
            );
        }
        return fn;
    }

    // Check if UIAccess is available for current process
    bool CheckUIAccess();
    
    // Acquire UIAccess token (requires admin privileges)
    // This duplicates winlogon.exe token to get UIAccess rights
    bool AcquireUIAccessToken();
    
    // Create overlay window with UIAccess (works on fullscreen)
    // Falls back to regular CreateWindowEx if UIAccess fails
    HWND CreateUIAccessWindow(
        DWORD dwExStyle,
        LPCWSTR lpClassName,
        LPCWSTR lpWindowName,
        DWORD dwStyle,
        int X, int Y, int nWidth, int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam
    );

    // Check if we're running with admin privileges
    bool IsRunningAsAdmin();

} // namespace uiaccess

