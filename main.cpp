#include <windows.h>
#include <iostream>

bool IsLightMode(HKEY hkey) {
    DWORD value = 1;
    DWORD size = sizeof(DWORD);

    RegQueryValueExW(
        hkey,
        L"AppsUseLightTheme",
        nullptr,
        nullptr,
        reinterpret_cast<LPBYTE>(&value),
        &size
    );

    return value == 0;
}

int main() {
    HKEY hkey = nullptr;

    LONG lResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_NOTIFY | KEY_READ,
        &hkey
    );

    if (lResult != ERROR_SUCCESS) {
        std::cerr<<"Failed to open registry key "<<lResult<<"\n";
        return 1;
    }

    std::cout<<"Current theme is:"<<((IsLightMode(hkey)) ? "dark" : "light") <<"\n";
    

    while (true) {
        lResult = RegNotifyChangeKeyValue(
            hkey,
            FALSE,
            REG_NOTIFY_CHANGE_LAST_SET,
            nullptr,
            FALSE
        );

        if (lResult != ERROR_SUCCESS) {
            std::cerr<<"Reg notify failed: "<<lResult<<"\n";
            break;
        }
        
        std::cout<<"Theme changed: "<<((IsLightMode(hkey)) ? "dark" : "light") <<"\n";
    }

    RegCloseKey(hkey);
    return 0;
}
