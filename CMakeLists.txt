cmake_minimum_required(VERSION 3.20)
project(MathX VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)



# ── Qt6 ───────────────────────────────────────────────────────────────────────
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64" CACHE PATH "Qt install root")
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ── Boost (header-only) ───────────────────────────────────────────────────────
set(BOOST_ROOT "C:/Local/Boost_1_90_0" CACHE PATH "Boost install root")

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    # Entry point
    src/main.cpp

    # Theme (header-only, no cpp)

    # Math library
    src/math/BigNum.cpp
    src/math/Expr.cpp
    src/math/Solver.cpp
    src/math/MathEngine.cpp

    # Shapes library
    src/shapes/GeoCard.cpp
    src/shapes/Shapes2D.cpp
    src/shapes/Shapes3D.cpp

    # Input library
    src/input/InputHandler.cpp

    # UI library
    src/ui/OutputArea.cpp
    src/ui/SidebarPanel.cpp
    src/ui/MainWindow.cpp

    # Qt resources
    resources/resources.qrc
)

set(HEADERS
    src/theme/Theme.h

    src/math/BigNum.h
    src/math/Expr.h
    src/math/Solver.h
    src/math/MathEngine.h

    src/shapes/GeoCard.h
    src/shapes/Shapes2D.h
    src/shapes/Shapes3D.h

    src/input/ShapePrompt.h
    src/input/InputHandler.h

    src/ui/OutputArea.h
    src/ui/SidebarPanel.h
    src/ui/MainWindow.h
 "src/shapes/ShapeDef.h")

if(WIN32)
    add_executable(MathX WIN32 ${SOURCES} ${HEADERS} "src/shapes/ShapeDef.h" "src/math/FactorialWorker.h" "src/math/FactorialWorker.cpp"  "src/ui/RunState.h" "src/input/PromptController.cpp" "src/math/tryConversion.cpp"  "src/ui/ClickableLabel.h"   "src/thread/GenericWorker.h" "src/thread/GenericWorker.cpp" "src/thread/PersistentWorker.h" "src/thread/PersistentWorker.cpp" "src/math/Simplifier.h" "src/math/NaturalLanguage.h" "src/math/NaturalLanguage.cpp" "src/math/Simplifier.cpp")
else()
    add_executable(MathX ${SOURCES} ${HEADERS})
endif()

target_link_libraries(MathX PRIVATE Qt6::Widgets)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(MathX PRIVATE
    "${BOOST_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ── MSVC flags ────────────────────────────────────────────────────────────────
target_compile_definitions(MathX PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    BOOST_DISABLE_CURRENT_LOCATION

)

# ── Post-build: deploy Qt DLLs ────────────────────────────────────────────────
if(WIN32)
    add_custom_command(TARGET MathX POST_BUILD
        COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
            --no-translations --no-system-d3d-compiler
            $<TARGET_FILE:MathX>
        COMMENT "Running windeployqt..." VERBATIM
    )
endif()
