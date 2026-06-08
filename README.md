# MATHX

**MathX** is a native desktop mathematics application written in C++ and built with Qt6.

It is a passion project developed by a single developer (me) with no budget, created as both a tool to help with mathematics and a way to challenge and improve my programming skills.

## Goals

* Help me explore and solve mathematical problems
* Test and improve my programming skills
* Build software that feels fast, intuitive, and low-friction
* Experiment with mathematical visualization and interaction

# Why C++

MathX is written in C++ because I enjoy working with native software.

C++ gives me direct control over the application's architecture, performance, threading model, rendering pipeline, and memory usage.

Since MathX is intended to be a desktop application, I also like that it can be compiled into a standalone executable without requiring users to install a separate runtime or interpreter.

While other languages such as Python are excellent tools and are widely used throughout the industry, C++ was the best fit for the type of application I wanted to build.


\---

### Prerequisites

|Tool|Notes|
|-|-|
|**Qt 6.x**|Install via Qt Online Installer. Choose `MSVC 2022 64-bit` kit.|
|**Visual Studio 2022**|With **Desktop development with C++** workload.|
|**CMake 3.20+**|Bundled with VS2022 or install separately.|

\---


## Tech Stack

* MSVC 2022
* Boost
* Qt6
* OpenGL
* C++
* CMake


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

MATHX uses **JetBrains Mono** for the terminal aesthetic, and uses **Syne ExtraBold** for the logo
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

### Features to be implemented:

* Statements
* Persistent Variables
* Alias Support
* better user Friendliness
* graphs and hyperbolic functions
* Notation support


## Background 

I started building MathX during my summer vacation with the goal of finishing a working prototype before my summer break ended.

I originally wanted a calculator that could solve for x and handle more advanced mathematical tasks than the tools I was already using.

As I kept adding features, the project gradually turned into something much larger and became one of my favorite programming projects.

As i kept building it, a philosophy formed around this project, that the user shouldn't have to use a 5-page manual to use it, it should instead come naturally to them.

The app should be as low-friction as possible, the two philosophies add technical complexity, but pay off for User Experience

