#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <fstream>
#include <optional>

#define CONFIG_FILE_PATH ".\\config.conf"

// Globals
HANDLE g_shutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

class RegKeyFailureException : public std::runtime_error {
    public:
    RegKeyFailureException(std::string message) : std::runtime_error(message) {}
};

struct WallpaperPath {
    std::optional<std::wstring> lightPath = std::nullopt;
    std::optional<std::wstring> darkPath = std::nullopt;
};

BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            SetEvent(g_shutdownEvent);
            return TRUE;
    }
    return FALSE;
}

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

void CloseKeys(HKEY hKeyTheme, HKEY hKeyWallpaper) {
    RegCloseKey(hKeyTheme);
    RegCloseKey(hKeyWallpaper);
}

BOOL ChangeWallpaper(std::wstring path) {
    return SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        path.data(),
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
    );
}

int main() {
    std::wifstream ifstream(CONFIG_FILE_PATH);

    WallpaperPath wallpaperPath;

    if (ifstream.is_open()) {
        std::wstring line;
        while (std::getline(ifstream, line)) {
            if (line.find(L"Dark=") == 0) {
                wallpaperPath.darkPath = line.substr(5, line.size());
            }
            if (line.find(L"Light=") == 0) {
                wallpaperPath.lightPath = line.substr(6, line.size());
            }
        }
    }

    ifstream.close();

    HKEY hKeyTheme = nullptr;
    HKEY hKeyWallpaper = nullptr;

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr<<"Unable to set console handler\n";
        return 1;
    }

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

    bool isDarkCurrent = false;
    std::wstring currentWallPaper;

    try {
        isDarkCurrent = IsLightMode(hKeyTheme);
        currentWallPaper = ReadWallpaperPath(hKeyWallpaper);
    } catch (RegKeyFailureException exeption) {
        std::cerr<<exeption.what()<<"\n";
        CloseKeys(hKeyTheme, hKeyWallpaper);
        return 1;
    }

    std::cout<<"Current theme is: "<<((isDarkCurrent) ? "dark" : "light") <<"\n";
    std::wcout<<"Current walpaper is"<<currentWallPaper<<L"\n";

    if (isDarkCurrent) {
        wallpaperPath.darkPath = currentWallPaper;
    } else {
        wallpaperPath.lightPath = currentWallPaper;
    }

    HANDLE hEventTheme = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HANDLE hEventWallpaper = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HANDLE handles[] = {hEventTheme, g_shutdownEvent, hEventWallpaper};

    while (true) {
        lResult = RegNotifyChangeKeyValue(
            hKeyTheme,
            FALSE,
            REG_NOTIFY_CHANGE_LAST_SET,
            hEventTheme,
            TRUE
        );

        if (lResult != ERROR_SUCCESS) {
            std::cerr<<"Reg notify failed: "<<lResult<<"\n";
            break;
        }

        lResult = RegNotifyChangeKeyValue(
            hKeyWallpaper,
            FALSE,
            REG_NOTIFY_CHANGE_LAST_SET,
            hEventWallpaper,
            TRUE
        );

        if (lResult != ERROR_SUCCESS) {
            std::cerr<<"Reg notify failed: "<<lResult<<"\n";
            break;
        }

        DWORD waitResult = WaitForMultipleObjects(
            3,
            handles,
            FALSE,
            INFINITE
        );

        if (WAIT_OBJECT_0 + 1 == waitResult) {
            std::cout<<"Bye...\n";
            break;
        }

        if (WAIT_OBJECT_0 == waitResult) {
            try {
                bool isDarkNew = IsLightMode(hKeyTheme);
                if (isDarkCurrent != isDarkNew) {
                    isDarkCurrent = isDarkNew;
                    std::cout<<"Theme changed to: "<<((isDarkCurrent) ? "dark\n" : "light\n");
                    if(isDarkCurrent && wallpaperPath.darkPath.has_value()) {
                        if (!ChangeWallpaper(wallpaperPath.darkPath.value())) {
                            std::cerr<<"Failed to update wallpaper";
                            break;
                        }
                    }
                    if(!isDarkCurrent && wallpaperPath.lightPath.has_value()) {
                        if (!ChangeWallpaper(wallpaperPath.lightPath.value())) {
                            std::cerr<<"Failed to update wallpaper";
                            break;
                        }
                    }
                }
            } catch (RegKeyFailureException exception) {
                std::cerr<<exception.what()<<"\n";
                break;
            }
        }

        if (WAIT_OBJECT_0 + 2 == waitResult) {
            try {
                std::wstring newWallpaper = ReadWallpaperPath(hKeyWallpaper);
                if (newWallpaper != currentWallPaper) {
                    currentWallPaper = newWallpaper;
                    std::wcout<<L"Wallpaper changed to:"<<currentWallPaper<<L"\n";
                    if (isDarkCurrent) {
                        wallpaperPath.darkPath = currentWallPaper;
                    } else {
                        wallpaperPath.lightPath = currentWallPaper;
                    }
                }
            } catch (RegKeyFailureException exception) {
                std::cerr<<exception.what()<<"\n";
                break;
            }
        }
    }

    CloseHandle(hEventTheme);
    CloseHandle(g_shutdownEvent);
    CloseKeys(hKeyTheme, hKeyWallpaper);

    std::wofstream ofstream(CONFIG_FILE_PATH);

    if (!ofstream.is_open()) {
        std::cerr<<"Can't write file";
        return 1;
    }

    if (wallpaperPath.darkPath.has_value()) {
        wallpaperPath.darkPath.value().erase(wallpaperPath.darkPath.value().find_first_of(L'\0') + 1);
        ofstream<<L"Dark="<<wallpaperPath.darkPath.value()<<L"\n";
    }

    if (wallpaperPath.lightPath.has_value()) {
        wallpaperPath.lightPath.value().erase(wallpaperPath.lightPath.value().find_first_of(L'\0') + 1);
        ofstream<<L"Light="<<wallpaperPath.lightPath.value()<<L"\n";
    }

    ofstream.close();
    return 0;
}
