cmake_minimum_required(VERSION 3.20)

project(MathX VERSION 1.0 LANGUAGES CXX)

# -- C++ Standard --------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -- Qt Auto-tools -------------------------------------------------------------
# These tell CMake to automatically run Qt's code generators:
#   AUTOMOC  → runs moc on any file with Q_OBJECT (required for signals/slots)
#   AUTOUIC  → runs uic on .ui files (Qt Designer forms)
#   AUTORCC  → runs rcc on .qrc resource files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# -- Qt6 -----------------------------------------------------------------------
#
#
# How to pass your Qt path:
#
#   Option A - CMake CLI:
#     cmake -B build -DQt6_DIR="C:/Qt/6.11.0/msvc2022_64/lib/cmake/Qt6"
#
#   Option B - set an environment variable called Qt6_DIR once on your machine,
#     then CMake will find it automatically every time.
#
#   Option C - still hardcode it, but in CMakeSettings.json or a
#     CMakeUserPresets.json (which is gitignored) so it never hits the repo.
#
# The line below just makes Qt6_DIR a configurable cache variable with a
# helpful description shown in CMake GUI / IDEs.
set(Qt6_DIR "" CACHE PATH "Path to Qt6 cmake dir, e.g. C:/Qt/6.11.0/msvc2022_64/lib/cmake/Qt6")

if(Qt6_DIR)
    # If the user supplied Qt6_DIR, prepend it to the search path.
    list(PREPEND CMAKE_PREFIX_PATH "${Qt6_DIR}/../../../")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets OpenGL)

# -- Boost (header-only) -------------------------------------------------------
# Pass -DBOOST_ROOT on the cmake command line.
# or set the BOOST_ROOT environment variable. 
set(BOOST_ROOT "" CACHE PATH "Path to your Boost installation root")

# -- Sources (common - all platforms) -----------------------------------------
set(COMMON_SOURCES
    src/main.cpp
    src/cli/BatchCli.cpp
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
    src/ui/FocusGlow.cpp
    src/ui/OutlineOverlay.cpp
    src/render/RenderWidget.cpp
    src/render/OpenGLRenderer.cpp
    src/ui/GeoModeWidget.cpp
    src/shapes/cube/cube.cpp
    src/shapes/sphere/sphere.cpp
    src/shapes/cylinder/cylinder.cpp
    src/shapes/cuboid/cuboid.cpp

    resources/resources.qrc
)

set(COMMON_HEADERS
    src/cli/BatchCli.h
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
    src/ui/FocusGlow.h
    src/ui/OutlineOverlay.h
    src/render/Renderer.h
    src/render/OpenGLRenderer.h
    src/render/VulkanRenderer.h
    src/render/RenderWidget.h
    src/ui/GeoModeWidget.h
    src/shapes/RenderShape.h
    src/shapes/ShapeInfo.h
    src/shapes/cube/cube.h
    src/shapes/sphere/sphere.h
    src/shapes/cylinder/cylinder.h
)

# -- Additional sources (all platforms) ---------------------------------------
# These once lived inside an if(WIN32) guard, but none of them contain
# platform-specific code - they are the math engine, natural-language layer,
# worker threads, and plain Qt widgets that main.cpp and MathEngine depend on.
# They MUST be compiled on every platform, or the non-Windows link fails with
# undefined references (e.g. NaturalLanguage::warmUp, PersistentWorker).
list(APPEND COMMON_SOURCES
    src/math/FactorialWorker.cpp
    src/input/PromptController.cpp
    src/math/tryConversion.cpp
    src/math/NaturalLanguage.cpp
    src/math/WordProblem.cpp
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

list(APPEND COMMON_HEADERS
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
    src/math/WordProblem.h
    src/math/Polynomial.h
    src/math/Algebra.h
)

# -- Executable ----------------------------------------------------------------
# WIN32 here means "make a GUI app (no console window)" on Windows.
# On other platforms it has no effect.
add_executable(MathX WIN32
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
 "src/settings/Settings.h" "src/settings/Settings.cpp" "src/settings/SettingsDef.h" "src/settings/SettingsDef.cpp" "src/ui/settings/SettingsPage.h" "src/ui/settings/SettingsPage.cpp" "src/shapes/ShapeRegistry.h" "src/settings/RegistryWatcher.h" "src/settings/RegistryWatcher.cpp" "src/constants/RunSource.h" "src/constants/StateUtils.h" "src/constants/StateUtils.cpp" "src/subSystems/WidgetRegistry.h" "src/subSystems/WidgetRegistry.cpp" "src/ui/Memusage.cpp" "src/ui/MemoryUsage.h" "src/ui/MemoryUsage.cpp" "src/constants/Theme.cpp" "src/ui/BatchRunner.h" "src/ui/BatchRunner.cpp" "src/ui/BatchPanel.h" "src/ui/BatchPanel.cpp" "src/shapes/ShapeRegistry.cpp" "src/ui/ModdedBtn.h" "src/ui/ModdedBtn.cpp" "src/ui/ApproxCard.h" "src/ui/ApproxCard.cpp" "src/ui/NumberFormat.h" "src/ui/NumberFormat.cpp" "src/shapes/ResultRow.h" "src/shapes/SliderRow.h" "src/shapes/ShapeDefs.h" "src/shapes/RenderMode.h" "src/ui/PropertiesPanel.h" "src/shapes/circle/circle.h" "src/shapes/circle/circle.cpp" "src/shapes/MeshShape.h" "src/shapes/MeshShape.cpp" "src/ui/CopyableLabel.h" "src/ui/CopyableLabel.cpp" "src/ui/MasterLabel.h" "src/ui/WidgetEditorPage.h" "src/ui/WidgetEditorPage.cpp" "src/math/PolyEngine.cpp" "src/math/PolyEngine.h" "src/math/ExprTree.cpp" "src/math/ExprTree.h" "src/math/TreeParser.cpp" "src/math/TreeParser.h" "src/math/TreePoly.cpp" "src/math/TreePoly.h" "src/math/TreeSolver.cpp" "src/math/TreeSolver.h" "src/ui/CopyableLabel.h" "src/ui/ExprTokens.cpp" "src/ui/ExprTokens.h" "src/ui/ScrubbableExpression.cpp" "src/ui/ScrubbableExpression.h" "src/ui/SessionPanel.cpp" "src/ui/SessionPanel.h" "src/constants/ResultTypes.h" "src/render/MathText.h" "src/render/MathText.cpp" "src/ui/FeatureViewerPage.h" "src/ui/FeatureViewerPage.cpp" "src/input/Completer.h" "src/ui/GhostLineEdit.h" "src/ui/DemoTyper.h")

# -- Link libraries ------------------------------------------------------------
# PRIVATE means: MathX uses these libraries, but if something else linked
# against MathX it wouldn't automatically inherit them.
target_link_libraries(MathX PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL

)

# OpenGL system libraries
if(WIN32)
    target_link_libraries(MathX PRIVATE opengl32 glu32 advapi32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(MathX PRIVATE OpenGL::GL)
    # CHANGE: use the modern imported target OpenGL::GL instead of the raw
    # ${OPENGL_LIBRARIES} variable - it's more robust across platforms.
endif()

# -- Include directories -------------------------------------------------------
target_include_directories(MathX PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Avoids a confusing "directory does not exist" warning when Boost isn't set.
if(BOOST_ROOT)
    target_include_directories(MathX PRIVATE "${BOOST_ROOT}")
endif()

# -- Compile definitions -------------------------------------------------------
target_compile_definitions(MathX PRIVATE
    BOOST_DISABLE_CURRENT_LOCATION    # Removes a Boost warning (all platforms)
)

if(WIN32)
    target_compile_definitions(MathX PRIVATE
        NOMINMAX                      # Prevent Windows.h from defining min/max macros
        WIN32_LEAN_AND_MEAN           # Trim down Windows.h includes for faster builds
    )
endif()


# -- windeployqt (Windows only) ------------------------------------------------
# This runs Qt's deployment tool after every build, which copies the Qt DLLs
# your .exe needs next to it, so you can run it without installing Qt globally.
if(WIN32)
    # CHANGE: use Qt6::windeployqt target instead of hardcoding the .exe path.
    # Qt6 provides this as a known target, so it works regardless of where Qt is installed.
    if(TARGET Qt6::windeployqt)
        add_custom_command(TARGET MathX POST_BUILD
            COMMAND Qt6::windeployqt
                --no-translations
                --no-system-d3d-compiler
                $<TARGET_FILE:MathX>
            COMMENT "Running windeployqt..."
        )
    else()
        # Fallback: if the target isn't available, try to find windeployqt manually
        find_program(WINDEPLOYQT windeployqt
            HINTS "${Qt6_DIR}/../../../bin"
        )
        if(WINDEPLOYQT)
            add_custom_command(TARGET MathX POST_BUILD
                COMMAND "${WINDEPLOYQT}"
                    --no-translations
                    --no-system-d3d-compiler
                    $<TARGET_FILE:MathX>
                COMMENT "Running windeployqt..."
            )
        else()
            message(WARNING "windeployqt not found - you may need to deploy Qt DLLs manually.")
        endif()
    endif()
endif()