/*
 * UIAccess Implementation
 * Based on: https://yougame.biz/threads/348163/
 * Source: https://github.com/killtimer0/uiaccess
 */

#include "uiaccess.hpp"
#include <TlHelp32.h>
#include <tchar.h>

#pragma comment(lib, "advapi32.lib")

namespace uiaccess {

    // Duplicate token from winlogon.exe to get TCB privilege
    static DWORD DuplicateWinlogonToken(DWORD dwSessionId, DWORD dwDesiredAccess, PHANDLE phToken) {
        DWORD dwErr = ERROR_NOT_FOUND;
        PRIVILEGE_SET ps;

        ps.PrivilegeCount = 1;
        ps.Control = PRIVILEGE_SET_ALL_NECESSARY;

        if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &ps.Privilege[0].Luid)) {
            return GetLastError();
        }

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return GetLastError();
        }

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        BOOL bFound = FALSE;

        for (BOOL bCont = Process32First(hSnapshot, &pe); bCont; bCont = Process32Next(hSnapshot, &pe)) {
            if (_tcsicmp(pe.szExeFile, TEXT("winlogon.exe")) != 0) {
                continue;
            }

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProcess) continue;

            HANDLE hToken;
            if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)) {
                BOOL fTcb;
                if (PrivilegeCheck(hToken, &ps, &fTcb) && fTcb) {
                    DWORD sid, dwRetLen;
                    if (GetTokenInformation(hToken, TokenSessionId, &sid, sizeof(sid), &dwRetLen) && sid == dwSessionId) {
                        bFound = TRUE;
                        if (DuplicateTokenEx(hToken, dwDesiredAccess, NULL, SecurityImpersonation, TokenImpersonation, phToken)) {
                            dwErr = ERROR_SUCCESS;
                        } else {
                            dwErr = GetLastError();
                        }
                    }
                }
                CloseHandle(hToken);
            }
            CloseHandle(hProcess);

            if (bFound) break;
        }

        CloseHandle(hSnapshot);
        return dwErr;
    }

    // Create a token with UIAccess flag set
    static DWORD CreateUIAccessTokenInternal(PHANDLE phToken) {
        DWORD dwErr;
        HANDLE hTokenSelf;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hTokenSelf)) {
            return GetLastError();
        }

        DWORD dwSessionId, dwRetLen;
        if (!GetTokenInformation(hTokenSelf, TokenSessionId, &dwSessionId, sizeof(dwSessionId), &dwRetLen)) {
            CloseHandle(hTokenSelf);
            return GetLastError();
        }

        HANDLE hTokenSystem;
        dwErr = DuplicateWinlogonToken(dwSessionId, TOKEN_IMPERSONATE, &hTokenSystem);
        
        if (dwErr == ERROR_SUCCESS) {
            if (SetThreadToken(NULL, hTokenSystem)) {
                if (DuplicateTokenEx(hTokenSelf, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, 
                                     NULL, SecurityAnonymous, TokenPrimary, phToken)) {
                    BOOL bUIAccess = TRUE;
                    if (!SetTokenInformation(*phToken, TokenUIAccess, &bUIAccess, sizeof(bUIAccess))) {
                        dwErr = GetLastError();
                        CloseHandle(*phToken);
                    }
                } else {
                    dwErr = GetLastError();
                }
                RevertToSelf();
            } else {
                dwErr = GetLastError();
            }
            CloseHandle(hTokenSystem);
        }

        CloseHandle(hTokenSelf);
        return dwErr;
    }

    bool CheckUIAccess() {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            return false;
        }

        BOOL fUIAccess = FALSE;
        DWORD dwRetLen;
        GetTokenInformation(hToken, TokenUIAccess, &fUIAccess, sizeof(fUIAccess), &dwRetLen);
        CloseHandle(hToken);

        return fUIAccess != FALSE;
    }

    bool AcquireUIAccessToken() {
        // Already have UIAccess
        if (CheckUIAccess()) {
            return true;
        }

        // Need admin to acquire UIAccess
        if (!IsRunningAsAdmin()) {
            return false;
        }

        HANDLE hToken;
        DWORD dwErr = CreateUIAccessTokenInternal(&hToken);
        
        if (dwErr != ERROR_SUCCESS) {
            return false;
        }

        // Create new process with UIAccess token
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        // Add flag to prevent infinite recursion
        wchar_t cmdLine[MAX_PATH + 32];
        swprintf_s(cmdLine, L"\"%s\" --uiaccess-child", exePath);

        BOOL success = CreateProcessAsUserW(
            hToken,
            NULL,
            cmdLine,
            NULL, NULL,
            FALSE,
            0,
            NULL, NULL,
            &si, &pi
        );

        CloseHandle(hToken);

        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            // Exit current process, child will continue with UIAccess
            ExitProcess(0);
        }

        return false;
    }

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
    ) {
        // Try CreateWindowInBand with UIAccess band
        auto pCreateWindowInBand = GetCreateWindowInBand();
        
        if (pCreateWindowInBand && CheckUIAccess()) {
            // Register window class and get atom
            WNDCLASSEXW wc = { sizeof(wc) };
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = hInstance;
            wc.lpszClassName = lpClassName;
            
            ATOM classAtom = RegisterClassExW(&wc);
            if (!classAtom) {
                // Class might already be registered
                classAtom = (ATOM)GetClassInfoExW(hInstance, lpClassName, &wc);
            }
            
            if (classAtom) {
                HWND hwnd = pCreateWindowInBand(
                    dwExStyle,
                    classAtom,
                    lpWindowName,
                    dwStyle,
                    X, Y, nWidth, nHeight,
                    hWndParent,
                    hMenu,
                    hInstance,
                    lpParam,
                    ZBID_UIACCESS  // UIAccess band - above fullscreen!
                );
                
                if (hwnd) {
                    return hwnd;
                }
            }
        }

        // Fallback to regular CreateWindowEx
        return CreateWindowExW(
            dwExStyle,
            lpClassName,
            lpWindowName,
            dwStyle,
            X, Y, nWidth, nHeight,
            hWndParent,
            hMenu,
            hInstance,
            lpParam
        );
    }

    bool IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        
        return isAdmin != FALSE;
    }

} // namespace uiaccess

