# MATHX — Qt6 C++ Port

Exact UI replica of my MATHX HTML calculator, built with Qt6 Widgets + MSVC.

\---

### Prerequisites

|Tool|Notes|
|-|-|
|**Qt 6.x**|Install via Qt Online Installer. Choose `MSVC 2022 64-bit` kit.|
|**Visual Studio 2022**|With **Desktop development with C++** workload.|
|**CMake 3.20+**|Bundled with VS2022 or install separately.|

\---

## Build Instructions (Visual Studio 2022)

### Option A — CMake GUI / VS Developer Command Prompt

```bat
:: Open "x64 Native Tools Command Prompt for VS 2022"

cd path\\to\\mathx-qt

:: Configure (replace Qt path with your actual installation)
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
      -DQt6\_DIR="C:\\Qt\\6.7.3\\msvc2022\_64\\lib\\cmake\\Qt6"

:: Build
cmake --build build --config Release

:: Run
build\\Release\\MathX.exe
```

### Option B — Open in Visual Studio directly

1. Open Visual Studio 2022
2. **File → Open → CMake...** → select `CMakeLists.txt`
3. In the CMake Settings (Project → CMake Settings), add:

```
   Qt6\_DIR = C:\\Qt\\6.7.3\\msvc2022\_64\\lib\\cmake\\Qt6
   ```

4. **Build → Build All**
5. Run `MathX.exe` from the output folder

### Option C — Qt Creator

1. Open Qt Creator
2. Open `CMakeLists.txt` as a project
3. Select the `MSVC 2022 64-bit` kit
4. Configure \& Build

\---

### Qt Path Examples

Adjust `Qt6\_DIR` to match your Qt installation:

|Qt version|Path|
|-|-|
|Qt 6.7.x|`C:\\Qt\\6.7.3\\msvc2022\_64\\lib\\cmake\\Qt6`|
|Qt 6.6.x|`C:\\Qt\\6.6.2\\msvc2022\_64\\lib\\cmake\\Qt6`|
|Qt 6.5.x|`C:\\Qt\\6.5.3\\msvc2022\_64\\lib\\cmake\\Qt6`|


### Features being worked on:

* Statements
* User-Defined Variables
* Proper Aliases 
* Actual 3d \&\& 2d shape visualization (OpenGL, vulkan, SDL, etc)
* Better User Friendliness
* Graphs
* Proper Multithreading
* Multi-core Implementation
* Adding support for various notations

