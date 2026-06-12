#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <fstream>
#include <optional>
#include <filesystem>
#include <sstream>

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

inline BOOL ChangeWallpaper(std::wstring path) {
    return SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        path.data(),
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
    );
}

std::filesystem::path GetConfigPath() {
    wchar_t path[FILENAME_MAX] = {0};
    GetModuleFileNameW(nullptr, path, FILENAME_MAX);
    return std::filesystem::path(path).parent_path().append(L"config.conf");
}

HKEY WrapperRegKeyOpen(LPCWSTR subKey) throw(RegKeyFailureException) {
    HKEY hkey = nullptr;
    LONG lResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        subKey,
        0,
        KEY_NOTIFY | KEY_READ,
        &hkey
    );
    if (lResult != ERROR_SUCCESS) {
        std::ostringstream sstream("Failed to open key with result: ");
        sstream<<lResult;
        throw RegKeyFailureException(sstream.str());
    }
    return hkey;
}

inline LONG WrapperSetNotifier(HKEY hkey, HANDLE hEvent) {
    return RegNotifyChangeKeyValue(
            hkey,
            FALSE,
            REG_NOTIFY_CHANGE_LAST_SET,
            hEvent,
            TRUE
        );
}

int main() {
    std::filesystem::path configFilePath = GetConfigPath();

    std::wifstream ifstream(configFilePath);

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

    try {
        hKeyTheme = WrapperRegKeyOpen(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
    } catch (RegKeyFailureException exception) {
        std::cerr<<"Failed to open registry key for theme "<<exception.what()<<"\n";
        return 1;
    }

    try {
        hKeyWallpaper = WrapperRegKeyOpen(L"Control Panel\\Desktop");
    } catch (RegKeyFailureException exception) {
        std::cerr<<"Failed to open registry key for theme "<<exception.what()<<"\n";
        return 1;
    }

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
        LONG lResult = WrapperSetNotifier(hKeyTheme, hEventTheme);

        if (lResult != ERROR_SUCCESS) {
            std::cerr<<"Reg notify failed: "<<lResult<<"\n";
            break;
        }

        lResult = WrapperSetNotifier(hKeyWallpaper, hEventWallpaper);

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

    std::wofstream ofstream(configFilePath);

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
