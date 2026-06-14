# Win Theme Shift

Simple app for handling if user changes their theme from light mode to dark and vice versa

## Instalation

Download archive from [Releases page](https://github.com/IvanRoudakov/win-theme-shift/releases) and extract to separate folder.

## Build

### Requirements

MSVC (Build tools for Visual Studio or Visual Studio itself) that supports C++ 17.

### Executable compilation

Run 
```batch
cmake -S . -B build
cmake --build build --config Release
```
 
*Et voila!* App is in build\Release folder.

## Usage

Change theme from light to dark (or vice versa) and setup wallpaper. Then when you swith back to this theme linked wallpaper would be set up.
