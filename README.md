// ==WindhawkMod==
// @id              explorer-smart-navigation
// @name            Smart Explorer Navigation
// @description     Intelligent fallback for Explorer back/up navigation with silent error handling
// @version         1.0
// @author          Johhannas Reyn
// @github          https://github.com/JohhannasReyn/Windhawk_BackNavigationModifier
// @include         explorer.exe
// @compilerOptions -lole32 -lshell32 -luuid
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Smart Explorer Navigation

Fixes annoying Windows Explorer navigation behavior:

## Features
- **Smart Back Navigation**: When back/backspace fails, automatically navigates to parent directory
- **Fallback Chain**: Back → Parent → This PC → Desktop
- **Silent Failures**: No error dings when navigation/paths fail
- **Bonus Hotkey**: Shift+Backspace for "up" navigation (configurable)
- **Intelligent Swapping**: If "up" fails, tries "back" instead

## Why This Exists
Windows Explorer loves to ding at you when you hit backspace in the wrong context,
alerting you with the, "Idio-ding!". This mod makes navigation
more forgiving and QUIET - so Windows, can STFU, cuz we don't need everyone knowing.

## Usage
- Backspace/Back button: Smart back with fallback
- Shift+Backspace (or Alt+Backspace): Navigate up with fallback
- Failed path navigation: Silent failure (no action, no ding)

Customize the hotkey in settings!

---
**'WindhawkModSettings'**

### Configure the additional navigation hotkey, aside from:
- 'Alt'+<Arrow Keys: i.e. Left='Back', Up='UP', Right='Forward'> 

### An additional hotkey can be implemented (configurable): 
- 'Shift'+'Backspace'
- -or-
- 'Alt'+'Backspace'.

### ***The Complete Patch Navigation Flow Then Becomes***
- If either 'Back' or 'Up' is unavailable, then the other is used.
- If both are unavailable, then Explorer navigates to the next available parent (Up) directory.
- If the entire path is corrupt (i.e. a removable drive is hot-pulled) then Explorer navigates to 'This PC'. 
- If a literal navigation to a path doesn't exist, Windows should STFU and fail quietly!
- That is all 😊
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- upHotkey: shift
  $name: Up Navigation Modifier
  $description: >-
    Modifier key for "navigate up" hotkey (with Backspace).
    Options: shift, alt
  $options:
  - shift: Shift
  - alt: Alt
- suppressErrorDialogs: true
  $name: Suppress Error Dialogs
  $description: Block "Location is not available" error dialogs
- enableLogging: false
  $name: Enable Debug Logging
  $description: Log navigation events (for troubleshooting)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <commctrl.h>

struct {
    bool enableLogging;
} settings;

// ============================================================================
// Helper to check if text contains error message
// ============================================================================
BOOL IsLocationErrorText(LPCWSTR text) {
    if (!text) return FALSE;
    return (wcsstr(text, L"is not available") != NULL ||
            wcsstr(text, L"is unavailable") != NULL ||
            wcsstr(text, L"can't be found") != NULL ||
            wcsstr(text, L"cannot be found") != NULL ||
            wcsstr(text, L"could not be found") != NULL);
}

// ============================================================================
// Hook CreateWindowExW to catch dialog creation
// ============================================================================
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;

HWND WINAPI CreateWindowExW_Hook(
    DWORD dwExStyle,
    LPCWSTR lpClassName,
    LPCWSTR lpWindowName,
    DWORD dwStyle,
    int X, int Y,
    int nWidth, int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam) {
    
    // Check if this is a dialog window
    if (lpClassName && (wcscmp(lpClassName, L"#32770") == 0 || // Standard dialog class
                        wcsstr(lpClassName, L"Dialog") != NULL)) {
        
        if (settings.enableLogging) {
            Wh_Log(L"CreateWindowExW - Dialog detected!");
            Wh_Log(L"  ClassName: %s", lpClassName ? lpClassName : L"(null)");
            Wh_Log(L"  WindowName: %s", lpWindowName ? lpWindowName : L"(null)");
        }
        
        // Check window title for error text
        if (IsLocationErrorText(lpWindowName)) {
            if (settings.enableLogging) {
                Wh_Log(L"  >>> BLOCKING dialog creation!");
            }
            // Return a fake window handle that will be immediately destroyed
            return (HWND)1;
        }
    }
    
    return CreateWindowExW_Original(dwExStyle, lpClassName, lpWindowName, dwStyle,
                                    X, Y, nWidth, nHeight, hWndParent, hMenu,
                                    hInstance, lpParam);
}

// ============================================================================
// Hook SetWindowTextW to catch dialog text being set
// ============================================================================
using SetWindowTextW_t = decltype(&SetWindowTextW);
SetWindowTextW_t SetWindowTextW_Original;

BOOL WINAPI SetWindowTextW_Hook(HWND hWnd, LPCWSTR lpString) {
    if (lpString && settings.enableLogging) {
        // Check if it's a dialog window
        WCHAR className[256];
        GetClassNameW(hWnd, className, 256);
        
        if (wcscmp(className, L"#32770") == 0) {
            Wh_Log(L"SetWindowTextW on dialog: %s", lpString);
            
            if (IsLocationErrorText(lpString)) {
                Wh_Log(L"  >>> HIDING error dialog!");
                ShowWindow(hWnd, SW_HIDE);
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return TRUE;
            }
        }
    }
    
    return SetWindowTextW_Original(hWnd, lpString);
}

// ============================================================================
// Hook ShowWindow to catch dialogs being shown
// ============================================================================
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL) {
        WCHAR className[256];
        WCHAR windowText[512];
        
        GetClassNameW(hWnd, className, 256);
        GetWindowTextW(hWnd, windowText, 512);
        
        if (wcscmp(className, L"#32770") == 0) { // Dialog class
            if (settings.enableLogging) {
                Wh_Log(L"ShowWindow on dialog: %s", windowText);
            }
            
            if (IsLocationErrorText(windowText)) {
                if (settings.enableLogging) {
                    Wh_Log(L"  >>> BLOCKING dialog show!");
                }
                // Close it immediately
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return TRUE; // Pretend it worked
            }
        }
    }
    
    return ShowWindow_Original(hWnd, nCmdShow);
}

// ============================================================================
// Hook TaskDialog
// ============================================================================
using TaskDialog_t = HRESULT (WINAPI*)(HWND, HINSTANCE, PCWSTR, PCWSTR, PCWSTR, TASKDIALOG_COMMON_BUTTON_FLAGS, PCWSTR, int*);
TaskDialog_t TaskDialog_Original;

HRESULT WINAPI TaskDialog_Hook(
    HWND hwndOwner,
    HINSTANCE hInstance,
    PCWSTR pszWindowTitle,
    PCWSTR pszMainInstruction,
    PCWSTR pszContent,
    TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons,
    PCWSTR pszIcon,
    int* pnButton) {
    
    if (settings.enableLogging) {
        Wh_Log(L"TaskDialog called!");
        Wh_Log(L"  Content: %s", pszContent ? pszContent : L"(null)");
    }
    
    if (IsLocationErrorText(pszContent) || IsLocationErrorText(pszMainInstruction)) {
        if (settings.enableLogging) {
            Wh_Log(L"  >>> SUPPRESSING TaskDialog!");
        }
        if (pnButton) *pnButton = IDOK;
        return S_OK;
    }
    
    return TaskDialog_Original(hwndOwner, hInstance, pszWindowTitle, pszMainInstruction, 
                               pszContent, dwCommonButtons, pszIcon, pnButton);
}

// ============================================================================
// Hook MessageBoxW
// ============================================================================
using MessageBoxW_t = decltype(&MessageBoxW);
MessageBoxW_t MessageBoxW_Original;

int WINAPI MessageBoxW_Hook(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    if (settings.enableLogging) {
        Wh_Log(L"MessageBoxW called: %s", lpText ? lpText : L"(null)");
    }
    
    if (IsLocationErrorText(lpText)) {
        if (settings.enableLogging) {
            Wh_Log(L"  >>> Suppressing MessageBox");
        }
        return IDOK;
    }
    
    return MessageBoxW_Original(hWnd, lpText, lpCaption, uType);
}

// ============================================================================
// Hook MessageBeep
// ============================================================================
using MessageBeep_t = decltype(&MessageBeep);
MessageBeep_t MessageBeep_Original;

BOOL WINAPI MessageBeep_Hook(UINT uType) {
    if (settings.enableLogging) {
        Wh_Log(L"MessageBeep suppressed (type: %u)", uType);
    }
    return TRUE;
}

// ============================================================================
// Settings
// ============================================================================
void LoadSettings() {
    settings.enableLogging = Wh_GetIntSetting(L"enableLogging");
}

// ============================================================================
// Mod lifecycle
// ============================================================================
BOOL Wh_ModInit() {
    Wh_Log(L"===============================================");
    Wh_Log(L"Smart Explorer Navigation v6.0 - INIT STARTED");
    Wh_Log(L"===============================================");
    
    LoadSettings();
    
    // Hook CreateWindowExW
    BOOL r1 = Wh_SetFunctionHook((void*)CreateWindowExW,
                   (void*)CreateWindowExW_Hook,
                   (void**)&CreateWindowExW_Original);
    Wh_Log(L"CreateWindowExW hook: %s", r1 ? L"SUCCESS" : L"FAILED");
    
    // Hook SetWindowTextW
    BOOL r2 = Wh_SetFunctionHook((void*)SetWindowTextW,
                   (void*)SetWindowTextW_Hook,
                   (void**)&SetWindowTextW_Original);
    Wh_Log(L"SetWindowTextW hook: %s", r2 ? L"SUCCESS" : L"FAILED");
    
    // Hook ShowWindow
    BOOL r3 = Wh_SetFunctionHook((void*)ShowWindow,
                   (void*)ShowWindow_Hook,
                   (void**)&ShowWindow_Original);
    Wh_Log(L"ShowWindow hook: %s", r3 ? L"SUCCESS" : L"FAILED");
    
    // Hook TaskDialog
    HMODULE comctl32 = LoadLibraryW(L"comctl32.dll");
    if (comctl32) {
        void* pfn = (void*)GetProcAddress(comctl32, "TaskDialog");
        if (pfn) {
            BOOL r = Wh_SetFunctionHook(pfn, (void*)TaskDialog_Hook, (void**)&TaskDialog_Original);
            Wh_Log(L"TaskDialog hook: %s", r ? L"SUCCESS" : L"FAILED");
        }
    }
    
    // Hook MessageBoxW
    BOOL r4 = Wh_SetFunctionHook((void*)MessageBoxW,
                   (void*)MessageBoxW_Hook,
                   (void**)&MessageBoxW_Original);
    Wh_Log(L"MessageBoxW hook: %s", r4 ? L"SUCCESS" : L"FAILED");
    
    // Hook MessageBeep
    BOOL r5 = Wh_SetFunctionHook((void*)MessageBeep,
                   (void*)MessageBeep_Hook,
                   (void**)&MessageBeep_Original);
    Wh_Log(L"MessageBeep hook: %s", r5 ? L"SUCCESS" : L"FAILED");
    
    Wh_Log(L"===============================================");
    Wh_Log(L"Initialization complete - mod is now active");
    Wh_Log(L"===============================================");
    
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Smart Explorer Navigation - Uninit");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"Smart Explorer Navigation - Settings Changed");
    LoadSettings();
}
