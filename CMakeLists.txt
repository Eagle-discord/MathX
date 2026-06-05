cmake_minimum_required(VERSION 3.20)
project(MathX VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ── Qt6 ───────────────────────────────────────────────────────────────────────
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64" CACHE PATH "Qt install root")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets OpenGL)

# ── Boost (header-only) ───────────────────────────────────────────────────────
set(BOOST_ROOT "C:/Local/Boost_1_90_0" CACHE PATH "Boost install root")

# ── Sources (common) ─────────────────────────────────────────────────────────
set(COMMON_SOURCES
    src/main.cpp
    src/math/BigNum.cpp
    src/math/Expr.cpp
    src/math/Solver.cpp
    src/math/MathEngine.cpp
    src/shapes/GeoCard.cpp
    src/shapes/Shapes2D.cpp
    src/shapes/Shapes3D.cpp
    src/input/InputHandler.cpp
    src/ui/OutputArea.cpp
    src/ui/SidebarPanel.cpp
    src/ui/MainWindow.cpp
    src/ui/OutputWidget.cpp
    src/ui/FocusAnchor.cpp
    
    src/ui/Animations.cpp
    resources/resources.qrc
)

set(COMMON_HEADERS
    src/constants/Theme.h
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
    src/ui/OutputWidget.h
    src/ui/FocusAnchor.h
    
    src/ui/Animations.h
    src/shapes/ShapeDef.h
)

if(WIN32)
    set(EXTRA_SOURCES
        src/math/FactorialWorker.cpp
        src/input/PromptController.cpp
        src/math/tryConversion.cpp
        src/math/NaturalLanguage.cpp
        src/math/Simplifier.cpp
        src/math/Polynomial.cpp
        src/math/Algebra.cpp
        src/ui/CRTTextLabel.cpp
        src/ui/HistoryDock.cpp
        src/ui/DraggableExpressionEdit.cpp
        src/ui/DraggableResultLabel.cpp
        src/thread/GenericWorker.cpp
        src/thread/PersistentWorker.cpp
    )
    set(EXTRA_HEADERS
        src/math/FactorialWorker.h
        src/constants/RunState.h
        src/ui/ClickableLabel.h
        src/ui/ConversionLabel.h
        src/ui/CRTLogoWidget.h
        src/ui/ScanLineOverlay.h
        src/ui/CRTTextLabel.h
        src/ui/HistoryDock.h
        src/ui/DraggableExpressionEdit.h
        src/ui/DraggableResultLabel.h
        src/thread/GenericWorker.h
        src/thread/PersistentWorker.h
        src/math/Simplifier.h
        src/math/NaturalLanguage.h
        src/math/Polynomial.h
        src/math/Algebra.h
    )
    list(APPEND COMMON_SOURCES ${EXTRA_SOURCES})
    list(APPEND COMMON_HEADERS ${EXTRA_HEADERS})
endif()

# Executable (always defined)
add_executable(MathX WIN32 ${COMMON_SOURCES} ${COMMON_HEADERS}   "src/ui/FocusGlow.cpp" "src/ui/FocusGlow.h"  "src/ui/OutlineOverlay.h" "src/ui/OutlineOverlay.cpp" "src/render/Renderer.h" "src/render/OpenGLRenderer.h" "src/render/VulkanRenderer.h" "src/render/RenderWidget.h" "src/render/RenderWidget.cpp" "src/render/OpenGLRenderer.cpp" "src/ui/GeoModeWidget.h" "src/ui/GeoModeWidget.cpp" "src/shapes/RenderShape.h" "src/shapes/cube/cube.h" "src/shapes/cube/cube.cpp" "src/shapes/sphere/sphere.h" "src/shapes/sphere/sphere.cpp")

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(MathX PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL
)
if(WIN32)
    target_link_libraries(MathX PRIVATE opengl32 glu32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(MathX PRIVATE ${OPENGL_LIBRARIES})
endif()

target_include_directories(MathX PRIVATE
    "${BOOST_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(MathX PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    BOOST_DISABLE_CURRENT_LOCATION
)

if(WIN32)
    add_custom_command(TARGET MathX POST_BUILD
        COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
            --no-translations --no-system-d3d-compiler
            $<TARGET_FILE:MathX>
        COMMENT "Running windeployqt..." VERBATIM
    )
endif()