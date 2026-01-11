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
- enableLogging: false
  $name: Enable Debug Logging
  $description: Log navigation events (for troubleshooting)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winstring.h>
#include <shobjidl.h>

#pragma comment(lib, "shlwapi.lib")

struct {
    PCWSTR upHotkey;
    bool enableLogging;
} settings;

// ============================================================================
// Hook MessageBeep to suppress error dings
// ============================================================================
using MessageBeep_t = decltype(&MessageBeep);
MessageBeep_t MessageBeep_Original;

BOOL WINAPI MessageBeep_Hook(UINT uType) {
    // Suppress all explorer error beeps
    if (settings.enableLogging) {
        Wh_Log(L"MessageBeep suppressed (type: %u)", uType);
    }
    return TRUE; // Pretend it succeeded, but do nothing
}

// ============================================================================
// Helper: Get IShellBrowser from window
// ============================================================================
IShellBrowser* GetShellBrowserFromHwnd(HWND hwnd) {
    IShellBrowser* psb = nullptr;
    
    // Try to get IShellBrowser via window property
    IUnknown* punk = (IUnknown*)GetProp(hwnd, L"CabinetWClass.IShellBrowser");
    if (punk) {
        punk->QueryInterface(IID_IShellBrowser, (void**)&psb);
    }
    
    return psb;
}

// ============================================================================
// Smart navigation fallback logic
// ============================================================================
bool NavigateToParent(IShellBrowser* psb) {
    if (!psb) return false;
    
    IShellView* psv = nullptr;
    if (SUCCEEDED(psb->QueryActiveShellView(&psv)) && psv) {
        IPersistFolder2* ppf2 = nullptr;
        if (SUCCEEDED(psv->GetItemObject(SVGIO_BACKGROUND, IID_IPersistFolder2, (void**)&ppf2)) && ppf2) {
            LPITEMIDLIST pidl = nullptr;
            if (SUCCEEDED(ppf2->GetCurFolder(&pidl)) && pidl) {
                LPITEMIDLIST pidlParent = ILClone(pidl);
                if (ILRemoveLastID(pidlParent)) {
                    HRESULT hr = psb->BrowseObject(pidlParent, SBSP_ABSOLUTE);
                    CoTaskMemFree(pidlParent);
                    CoTaskMemFree(pidl);
                    ppf2->Release();
                    psv->Release();
                    return SUCCEEDED(hr);
                }
                CoTaskMemFree(pidlParent);
                CoTaskMemFree(pidl);
            }
            ppf2->Release();
        }
        psv->Release();
    }
    return false;
}

bool NavigateToThisPC(IShellBrowser* psb) {
    if (!psb) return false;
    
    LPITEMIDLIST pidl = nullptr;
    if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl)) && pidl) {
        HRESULT hr = psb->BrowseObject(pidl, SBSP_ABSOLUTE);
        CoTaskMemFree(pidl);
        return SUCCEEDED(hr);
    }
    return false;
}

bool NavigateToDesktop(IShellBrowser* psb) {
    if (!psb) return false;
    
    LPITEMIDLIST pidl = nullptr;
    if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, CSIDL_DESKTOP, &pidl)) && pidl) {
        HRESULT hr = psb->BrowseObject(pidl, SBSP_ABSOLUTE);
        CoTaskMemFree(pidl);
        return SUCCEEDED(hr);
    }
    return false;
}

bool SmartNavigateBack(IShellBrowser* psb) {
    if (!psb) return false;
    
    // Try normal back first
    HRESULT hr = psb->BrowseObject(nullptr, SBSP_NAVIGATEBACK);
    if (SUCCEEDED(hr)) {
        if (settings.enableLogging) Wh_Log(L"Normal back succeeded");
        return true;
    }
    
    // Back failed, try parent
    if (settings.enableLogging) Wh_Log(L"Back failed, trying parent");
    if (NavigateToParent(psb)) return true;
    
    // Parent failed, try This PC
    if (settings.enableLogging) Wh_Log(L"Parent failed, trying This PC");
    if (NavigateToThisPC(psb)) return true;
    
    // This PC failed, try Desktop
    if (settings.enableLogging) Wh_Log(L"This PC failed, trying Desktop");
    return NavigateToDesktop(psb);
}

bool SmartNavigateUp(IShellBrowser* psb) {
    if (!psb) return false;
    
    // Try parent first
    if (NavigateToParent(psb)) {
        if (settings.enableLogging) Wh_Log(L"Navigate up succeeded");
        return true;
    }
    
    // Parent failed, try back instead
    if (settings.enableLogging) Wh_Log(L"Navigate up failed, trying back");
    return SmartNavigateBack(psb);
}

// ============================================================================
// Hook keyboard input to intercept backspace
// ============================================================================
using TranslateAcceleratorW_t = BOOL (WINAPI*)(HWND, HACCEL, LPMSG);
TranslateAcceleratorW_t TranslateAcceleratorW_Original;

BOOL WINAPI TranslateAcceleratorW_Hook(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg) {
    // Only process WM_KEYDOWN
    if (lpMsg->message == WM_KEYDOWN) {
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        
        // Check for our hotkeys
        if (lpMsg->wParam == VK_BACK && !isCtrl) {
            IShellBrowser* psb = GetShellBrowserFromHwnd(hWnd);
            if (psb) {
                bool handled = false;
                
                // Check for up navigation hotkey
                if ((wcscmp(settings.upHotkey, L"shift") == 0 && isShift) ||
                    (wcscmp(settings.upHotkey, L"alt") == 0 && isAlt)) {
                    if (settings.enableLogging) Wh_Log(L"Up hotkey detected");
                    handled = SmartNavigateUp(psb);
                }
                // Plain backspace = smart back
                else if (!isShift && !isAlt) {
                    if (settings.enableLogging) Wh_Log(L"Backspace detected");
                    handled = SmartNavigateBack(psb);
                }
                
                psb->Release();
                
                if (handled) {
                    return TRUE; // Prevent default behavior
                }
            }
        }
    }
    
    // Fall through to original
    return TranslateAcceleratorW_Original(hWnd, hAccTable, lpMsg);
}

// ============================================================================
// Settings management
// ============================================================================
void LoadSettings() {
    settings.upHotkey = Wh_GetStringSetting(L"upHotkey");
    settings.enableLogging = Wh_GetIntSetting(L"enableLogging");
}

// ============================================================================
// Mod lifecycle
// ============================================================================
BOOL Wh_ModInit() {
    Wh_Log(L"Smart Explorer Navigation - Init");
    
    LoadSettings();
    
    // Hook MessageBeep to suppress error dings
    Wh_SetFunctionHook((void*)MessageBeep,
                       (void*)MessageBeep_Hook,
                       (void**)&MessageBeep_Original);
    
    // Hook keyboard translation
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        void* pTranslateAcceleratorW = (void*)GetProcAddress(user32, "TranslateAcceleratorW");
        if (pTranslateAcceleratorW) {
            Wh_SetFunctionHook(pTranslateAcceleratorW,
                               (void*)TranslateAcceleratorW_Hook,
                               (void**)&TranslateAcceleratorW_Original);
        }
    }
    
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Smart Explorer Navigation - Uninit");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"Smart Explorer Navigation - Settings Changed");
    LoadSettings();
}
