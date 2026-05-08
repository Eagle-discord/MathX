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

\---

### Font

MATHX uses **JetBrains Mono** for the terminal aesthetic, and uses **Syne** for the logo
Qt will fall back to **Consolas** → **Courier New** automatically on Windows.

To embed the font:

1. Download `JetBrainsMono-Regular.ttf` from [JetBrains](https://www.jetbrains.com/lp/mono/)
2. Place in `resources/fonts/`
3. Create `resources/resources.qrc`:

```xml
   <RCC>
     <qresource prefix="/">
       <file>fonts/JetBrainsMono-Regular.ttf</file>
     </qresource>
   </RCC>
   ```

4. Add to CMakeLists.txt: `qt6\_add\_resources(MathX "resources/resources.qrc")`
5. Uncomment the `addApplicationFont` line in `main.cpp`

\---

### Features Ported

* ✅ Dark terminal UI with exact color palette
* ✅ macOS-style traffic-light dots in title bar
* ✅ Mode tabs: All / Arithmetic / Algebra / Trig / Geometry / Convert
* ✅ Scrollable output with color-coded result types
* ✅ Arithmetic: +, -, \*, /, ^, sqrt, cbrt, abs, log, ln, fact, nCr, nPr, gcd, lcm
* ✅ Algebra: linear equations, quadratic equations (real + complex roots), Newton solver
* ✅ Trig: sin/cos/tan/asin/acos/atan in degrees; sinr/cosr/tanr in radians
* ✅ Geometry: triangle, circle, rectangle, square, sphere, cylinder, cone
* ✅ Unit conversion: length, mass, temperature, volume, data, speed
* ✅ Sidebar quick-reference (click to fill, double-click to run)
* ✅ Session counter + last result display
* ✅ Clear history button
* ✅ Arrow-up for last command
* ✅ Enter key to run





### Features to be implemented:

* Statements
* Persistent Variables
* Proper Aliases 
* Actual 3d \&\& 2d shape visualization (OpenGL, vulkan, etc)
* Better User Friendliness
* Graphs
* Proper Multithreading
* Implementing Multi-cores
* Refining existing implementations
* Adding support for various notations

