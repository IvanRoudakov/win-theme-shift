#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>

class RegKeyFailureException : public std::runtime_error {
    public:
    RegKeyFailureException(std::string message) : std::runtime_error(message) {}
};


bool IsLightMode(HKEY hkey) throw(RegKeyFailureException) {
    DWORD value = 1;
    DWORD size = sizeof(DWORD);

    LONG lResult = RegQueryValueExW(
        hkey,
        L"AppsUseLightTheme",
        nullptr,
        nullptr,
        reinterpret_cast<LPBYTE>(&value),
        &size
    );

    if (lResult != ERROR_SUCCESS) {
        throw RegKeyFailureException("Failed to query 'AppsUseLightTheme'");
    }

    return value == 0;
}

std::wstring ReadWallpaperPath(HKEY hkey) throw(RegKeyFailureException) {
    LPWSTR const keyName = L"WallPaper";

    std::wstring value;
    DWORD type = 0;
    DWORD size = 0;

    LONG lResult = RegGetValueW(
        hkey,
        nullptr,
        keyName,
        RRF_RT_REG_SZ,
        &type,
        nullptr,
        &size
    );

    if (lResult != ERROR_SUCCESS) {
        throw RegKeyFailureException("Failed to get 'Wallpaper'");
    }

    value.resize(size / sizeof(wchar_t));

    lResult = RegGetValueW(
        hkey,
        nullptr,
        keyName,
        RRF_RT_REG_SZ,
        &type,
        &value[0],
        &size
    );

    if (lResult != ERROR_SUCCESS) {
        throw RegKeyFailureException("Failed to get 'Wallpaper'");
    }

    return value;
}

int main() {
    HKEY hKeyTheme = nullptr;
    HKEY hKeyWallpaper = nullptr;

    LONG lResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_NOTIFY | KEY_READ,
        &hKeyTheme
    );

    if (lResult != ERROR_SUCCESS) {
        std::cerr<<"Failed to open registry key for theme "<<lResult<<"\n";
        return 1;
    }

    lResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Control Panel\\Desktop",
        0,
        KEY_NOTIFY | KEY_READ,
        &hKeyWallpaper
    );

    try {
        std::cout<<"Current theme is: "<<((IsLightMode(hKeyTheme)) ? "dark" : "light") <<"\n";
        std::wcout<<L"Current wallpaper is: "<<(ReadWallpaperPath(hKeyWallpaper));
    } catch (RegKeyFailureException exeption) {
        std::cerr<<exeption.what()<<"\n";
        //TODO: add processing RegCloseKey
        return 1;
    }

    // while (true) {
    //     lResult = RegNotifyChangeKeyValue(
    //         hKeyTheme,
    //         FALSE,
    //         REG_NOTIFY_CHANGE_LAST_SET,
    //         nullptr,
    //         FALSE
    //     );

    //     if (lResult != ERROR_SUCCESS) {
    //         std::cerr<<"Reg notify failed: "<<lResult<<"\n";
    //         break;
    //     }
        
    //     std::cout<<"Theme changed: "<<((IsLightMode(hKeyTheme)) ? "dark" : "light") <<"\n";
    // }

    RegCloseKey(hKeyTheme);
    RegCloseKey(hKeyWallpaper);
    return 0;
}
