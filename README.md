# MATHX

**MathX** is a native desktop mathematics application written in C++ and built with Qt6.

It is a passion project developed by a single developer (me) with no budget, created as both a tool to help with my mathematics and a way to challenge and improve my programming skills.

## Goals

* Help me explore and solve mathematical problems, both for me personally and kids
* Test and improve my programming skills
* Build software that feels fast, intuitive, and low-friction
* Experiment with mathematical visualization and interaction, and ones that are fun to play with

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


## Building on Windows (Visual Studio 2022)

### Option A - CMake GUI / VS Developer Command Prompt

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

### Option B - Open in Visual Studio directly

1. Open Visual Studio 2022
2. **File → Open → CMake...** → select `CMakeLists.txt`
3. In the CMake Settings (Project → CMake Settings), add:

```
   Qt6\_DIR = C:\\Qt\\6.7.3\\msvc2022\_64\\lib\\cmake\\Qt6
   ```

4. **Build → Build All**
5. Run `MathX.exe` from the output folder

### Option C - Qt Creator

1. Open Qt Creator
2. Open `CMakeLists.txt` as a project
3. Select the `MSVC 2022 64-bit` kit
4. Configure \& Build

\---

## Building on Linux

MathX builds on any Linux distribution - it depends only on a C++20 compiler,
CMake, Qt 6, Boost (header-only), and OpenGL. There is no Windows-specific code
in the build; the few platform bits (registry watching, process-memory readout)
are `#ifdef`-guarded and degrade gracefully.

### 1. Install the dependencies

You need: **a C++20 compiler** (GCC ≥ 10 or Clang ≥ 12), **CMake ≥ 3.20**,
**Ninja** (or Make), **Qt 6** (Core, Gui, Widgets, OpenGL, OpenGLWidgets),
**Boost** headers (Multiprecision), and **OpenGL** dev libraries.

**Debian / Ubuntu / Mint / Pop!\_OS**
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
     qt6-base-dev libqt6opengl6-dev qt6-base-dev-tools \
     libboost-dev libgl1-mesa-dev
```

**Fedora / RHEL**
```bash
sudo dnf install gcc-c++ cmake ninja-build \
     qt6-qtbase-devel boost-devel mesa-libGL-devel
```

**Arch / Manjaro / EndeavourOS**
```bash
sudo pacman -S base-devel cmake ninja qt6-base boost mesa
```

**openSUSE**
```bash
sudo zypper install gcc-c++ cmake ninja \
     qt6-base-devel libboost_headers-devel Mesa-libGL-devel
```

> Distro not listed? Any package set providing the six dependencies above works -
> the package names differ but the CMake build is identical. For a fully
> distro-independent Qt, see the aqtinstall fallback below.

### 2. Configure and build

With Qt installed from your distro, CMake finds it automatically:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MathX
```

If Qt lives somewhere non-standard (e.g. the official Qt installer, or the
aqtinstall fallback), point CMake at it:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.0/gcc_64"
```

If Boost isn't in a standard location, add `-DBOOST_ROOT=/path/to/boost`.

### Distro-independent Qt (aqtinstall)

To avoid relying on distro Qt packages entirely, install Qt binaries anywhere:

```bash
pip install aqtinstall
aqt install-qt linux desktop 6.11.0 gcc_64 -O "$HOME/Qt"
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.0/gcc_64"
cmake --build build
```

### Notes

* **Running** requires a graphical session (X11 or Wayland); the Qt platform
  plugin comes from your Qt install.
* **Settings** are stored via `QSettings` - on Linux that's an INI file at
  `~/.config/MathX.conf` (the Windows build uses the registry instead). Live
  external-edit detection is Windows-only; in-app settings changes work
  everywhere.

\---

### Command-line batch mode (`--run-batch`)

Run a file of expressions straight through the math engine - no window - and
exit with a pass/fail code. This is the headless counterpart to the in-app
batch panel, meant for the regression suite and CI.

```bat
MathX.exe --run-batch tests\mathx_regression.txt
:: or:  MathX.exe --run-batch=tests\mathx_regression.txt
```

On Linux/macOS the binary is a normal console app, so output goes straight to
the terminal - no redirection needed:

```bash
./MathX --run-batch tests/mathx_regression.txt
```

* **File format** (same as the in-app batch panel): one expression per line;
  blank lines and lines starting with `#` are skipped; an inline `#` and
  everything after it is stripped.
* **Assertions** use the existing equality feature - write `2+2 = 4` and the
  engine reports `LHS = RHS  ✓` (pass) or `LHS ≠ RHS  ✗` (fail).
* Passing lines print one quiet `✓` line; failures and errors print the full
  result so the reason is visible.
* **Exit codes:** `0` = every line passed · `1` = at least one error or failed
  assertion · `2` = usage / file error.

The executable is a GUI-subsystem app, so it attaches to the parent terminal's
console for output. To capture it from PowerShell, redirect stdout:

```powershell
Start-Process .\MathX.exe -ArgumentList '--run-batch','suite.txt' `
    -Wait -NoNewWindow -RedirectStandardOutput out.txt
```

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

### Present Features

* Arithmetic (`+`, `-`, `*`, `/`, `%`, `^`)
* Implicit multiplication (`2x`, `3(x+1)`)
* Trigonometry (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh` and radian variants)
* Algebra - linear and quadratic equation solving (`88x = 704`, `x^2 + 5x + 6 = 0`)
* Polynomial simplification (`2x + 3x + (4x - x)` → `8x`)
* Arbitrary precision arithmetic - big integers and big decimals via Boost.Multiprecision
* Exact big factorials (`fact(1000)`, `1000!`) with progress reporting
* Exact big exponentiation (`2^1000`) with cancellation support
* Built-in functions: `sqrt`, `cbrt`, `abs`, `log`, `ln`, `log2`, `logbase`, `floor`, `ceil`, `round`, `exp`, `sign`, `min`, `max`, `pow`, `mod`, `hypot`, `gcd`, `lcm`, `ncr`, `npr`
* Unit conversion [WIP] - over 200 units across 30+ categories including:
  * Length, mass, time, area, volume, speed, pressure, energy, power, force, angle, temperature, frequency, data storage, fuel economy, torque, and more
  * Full SI prefix support (auto-generated: `km`, `mm`, `nm`, `GHz`, `kPa`, etc.)
  * Non-linear conversions (Celsius, Fahrenheit, Kelvin, Rankine, Delisle, and more)
  * Compound unit conversions (`km/h to m/s`, `ft/s to mph`)
  * Natural input (`100 km to miles`, `72 fahrenheit to celsius`, `mach 2 to km/h`)
* Natural language preprocessing (`"half of 80"`, `"3 times 7"`, `"5 percent of 200"`)
* Interactive geometry cards for 30+ shapes with full calculations:
  * 2D: circle, ellipse, triangle, right triangle, rectangle, square, parallelogram, trapezoid, rhombus, regular polygon, sector, annulus
  * 3D: sphere, hemisphere, cylinder, hollow cylinder, cone, frustum, cube, cuboid, tetrahedron, octahedron, icosahedron, dodecahedron, prism, pyramid, torus, ellipsoid, capsule
* Interactive parameter prompts - type a shape name and the app guides you through each value
* 3D OpenGL shape viewer with interactive rendering [WIP]
* Async computation - heavy calculations run on a background thread and can be cancelled
* History navigation (↑/↓ arrow keys)
* Right-click to copy any result
* Double-click result for conversion formula details
* Session panel (calculation count, last expression, last result)
* Quick Reference sidebar with one-click and double-click to run examples


### Features to be implemented (future commits):

* Statements
* Persistent Variables
* Command Aliases
* better user Friendliness
* graphs and hyperbolic functions
* Notation support
* projections for all shapes
* Shape Property visibility in the Geometry visualiser (radius, diameter, they should be visible) 
* option based dynamic color adjustment
* Remove the lines in circular shapes
* Impement word problems 
* Option to change fonts in the calculator
* Less chunky progress bar 
* Multiple shapes in one view
* adding finances (such as Intrest, loans, GST, Tarrifs, etc.)
* Formula Derivations
* Geometry measurements overlay
* Cross section mode
* Shape nets
* Unit support
* Shape comparison mode (side-by-side shapes)
* Image export support
* Themes
* Settings
* Feature Viewer
* Explainations (for things like the volume formula)


## Background 

I started building MathX during my summer vacation with the goal of finishing a working prototype before my summer break ended.

I originally wanted a calculator that could solve for x and handle more advanced mathematical tasks than the tools I was already using.

As I kept adding features, the project gradually turned into something much larger and became one of my favorite programming projects.

As i kept building it, a philosophy formed around this project, that the user shouldn't have to use a 5-page manual to use it, it should instead come naturally to them.

The app should be as low-friction as possible, the two philosophies add technical complexity, but pay off for User Experience


### Ideas


 //this is where my ideas go, at the end of my dev session, i will move it to future commit ideas




