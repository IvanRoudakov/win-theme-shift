# Win Theme Shift

Simple app for handling if user changes their theme from light mode to dark and vice versa

## Build

### Requirements

MSVC (Build tools for Visual Studio or Visual Studio itself) that supports C++ 17.

### Executable compilation

Run `cl.exe /EHsc /std:c++17 main.cpp /Fe:win-theme-shift.exe /link advapi32.lib user32.lib`
 
*Et voila!*

## Usage

Change theme from light to dark (or vice versa) and setup wallpaper. Then when you swith back to this theme linked wallpaper would be set up.
