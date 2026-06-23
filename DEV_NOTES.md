# MATHX – Architecture & Implementation Notes

This document describes the internal design of MATHX, a multi-threaded calculator with support for arithmetic, algebra, unit conversion, interactive geometry, and a 3D shape viewer. It is intended for maintainers and as a reference for future development.
# DEV_NOTES.md – Known Issues & Future Work

This document tracks known bugs (with reproduction steps and fix theories) and planned features for MATHX.  
Last updated: 2025-06-15

---

## ?? BUG-001 – Draggable Result Labels: number replacement fails on multiple drags  
**Date found:** 2025-06-07  
**Recreation:**  
1. Enter `2+2` ? result `4`.  
2. Drag the result `4` to the right ? number increases, expression updates to `2+4` ? result `6`.  
3. Drag again the new result `6` ? the number in the expression is replaced incorrectly (appending instead of replacing, e.g., `2+46`).  

**Theory for fix:**  
The stored original expression and the number substring are not updated after the first drag. In `OutputWidget::rebuildExpression`, we must update `m_originalExpr` and `m_currentNumber` to the new values after re?evaluation. Ensure the drag starts with the current displayed number (which is the result, not the original operand). A clean solution is to treat the result as the only draggable quantity and replace it directly in the original expression (which works only if the original expression is a single number). Better to implement `DraggableResultLabel` as a separate class that works only for pure arithmetic expressions where the result equals the number in the expression.

---

## ?? BUG-002 – Mouse rotation direction feels inverted when auto-rotation is off  
**Date found:** 2025-06-08  
**Recreation:**  
1. Disable auto?rotation (checkbox unchecked).  
2. Click and drag the shape horizontally ? shape rotates opposite to mouse movement.  

**Theory for fix:**  
The sign in `mouseMoveEvent` was incorrect. Correct mapping:  
`m_yRot += delta.x() * 0.5f;` (right ? rotate right)  
`m_xRot -= delta.y() * 0.5f;` (down ? rotate down).  

---

## ?? BUG-003 – Prompt handles `b = a` incorrectly (does not evaluate previous parameter)  
**Date found:** 2025-06-05  
**Recreation:**  
1. Enter `tri a = 34 b = a`.  
2. After setting `a=34`, the prompt asks for `b`. Typing `a` is rejected.  

**Theory for fix:**  
The prompt input evaluation should use the existing parameter values (stored in `m_promptVars`) when evaluating expressions. Modify `handlePromptInput` to first try `MathEngine::evalWith(value, m_promptVars, ok)`. This will resolve `a` to its current value. The code now has a `VarMap m_promptVars` and uses it in `handlePromptInput` – verify it works.

---

## ?? BUG-003 – Settings page does not apply changes correctly (maybe)  
**Date found:** 2026-06-15  
**Recreation:** (To be verified)  
1. Open Settings page.  
2. Change a setting (e.g., font size).  
3. Navigate away – does the change apply?  

**Theory for fix:**  
The apply pipeline uses `Settings::applyPending()` called when run state becomes idle or on navigation. Ensure `SettingsPage::prepareToLeave` triggers `applyPending(true)` and waits for the callback. The code seems correct; test thoroughly.

## BUG-004 - RegistryWatcher startup freeze

**Symptom**:
- Application freezes during startup.
- Runtime counter remains at 0s.
- UI becomes unresponsive.

**Cause**:
- RegistryWatcher was created with a QObject parent and later moved to a worker thread.
- Qt does not allow moving QObjects that already have a parent.

**Theory for Fix**:
- Create RegistryWatcher without a parent before calling moveToThread().
- Use deleteLater() for cleanup when the worker thread exits.

**Future bug Prevention**:
- Verify all worker-thread QObjects have no parent before moveToThread().

---

## ? Feature Wishlist (Not bugs)

- **Persistent variables** – allow `x = 5` and reuse `x` in later expressions.  
- **Graph plotting** – plot functions like `y = x^2` using a separate widget.  
- **History dock** – store previous expressions with double?click to re?evaluate.  
- **3D lighting & solid shading** – replace wireframe with lit, solid faces.  
- **Export geometry mode screenshot** – save current 3D view as PNG.  
- **Settings import/export** – backup and restore user preferences.  
- **Multi?language support** – i18n via Qt `.ts` files.  
- **Theming (light/dark)** – full light theme support (currently dark only).  
- **Undo/redo** – for expression input and shape parameter changes.  

---

## ? Recently Fixed


---

*Use `--bug` tags in commit messages to reference these IDs. For example: `git commit -m "Fix BUG-005: evaluate prompt variables"`*

---

## 1. Overall Structure

MATHX is a Qt 6 application written in C++17. The code is organised into several directories:

| Directory       | Responsibility                                                                 |
|----------------|--------------------------------------------------------------------------------|
| `src/constants` | Theme colours, run state enumeration.                                          |
| `src/input`     | Input handling, history navigation, shape prompting.                           |
| `src/math`      | Expression evaluation (`Expr`), big numbers (`BigNum`), equation solving (`Solver`), unit conversion (`tryConversion`), natural language (`NaturalLanguage`), polynomial simplifier (`Polynomial`), algebra (`Algebra`). |
| `src/shapes`    | Interactive geometry cards (`GeoCard`, `Shapes2D`, `Shapes3D`) and 3D shape classes (`RenderShape` hierarchy). |
| `src/thread`    | Persistent background worker (`PersistentWorker`) for all calculations.        |
| `src/ui`        | Main window, output area, sidebar, focus glow, draggable expression edit, animations, etc. |
| `src/render`    | Abstract renderer, OpenGL implementation (`OpenGLRenderer`), Vulkan stub, and widget wrapper (`RenderWidget`). |

All Qt signals/slots are used extensively for cross?thread communication. The application uses **one persistent worker thread** (except for the factorial?specific cancellation, which is handled inside `BigNum` via a callback).

---

## 2. Math Engine – Core Evaluation

### 2.1 Expression Parser (`Expr.cpp` / `Expr.h`)

- **Tokeniser** – reads the input string, recognises numbers, identifiers, operators, parentheses, postfix `!` and `%`.  
- **Recursive descent parser** – implements precedence (`+`, `-` lowest, then `*`, `/`, `%`, then `^`, then unary `+`/`-`, then postfix operators, then primary).  
- **Variables** – passed via a `VarMap` (QMap<QString, double>).  
- **Functions** – dispatched to `callBuiltin`, which contains all mathematical functions (trig, hyperbolic, log, factorial, combinatorics, etc.).  
- **Error handling** – unknown identifiers now throw an exception (instead of returning `0`). The exception is caught in `evalWith` and sets `ok = false`.

**Preprocessing steps** (in `evalWith`):
- Normalise brackets (`[`?`(`, etc.)
- Replace `**` with `^`
- Replace `x` as multiplication (`replaceXWithTimes`)
- Insert implicit multiplication (`insertImplicitMul`) – e.g., `2(3+4)` ? `2*(3+4)`

### 2.2 Unit Conversion (`tryConversion.cpp`)

- **Unit database** – built once into a `QMap<QString, ResolvedUnit>` from three tables: `SI_BASES` (prefixable), `MANUAL` (non?SI), `NONLINEAR` (temperature). Prefixes are automatically generated for SI bases.  
- **Resolution** – a unit expression may be simple (`"km"`) or compound (`"km/h"`). The compound parser splits numerator/denominator lists and computes the combined factor.  
- **Temperature** – uses explicit formulas (e.g., `°F = (°C × 9/5) + 32`) because linear factors are not meaningful.  
- **Raw conversion** – `100 km` alone converts to a default unit (e.g., `m`).  
- **Mach special case** – `mach 1` is rewritten to `1 mach` before parsing.  

### 2.3 Equation Solving (`Algebra.cpp` + `Solver.cpp`)

- **Linear equations** – fast method: evaluate at x=0,1,2 to obtain coefficients `a` and `b`.  
- **Quadratic equations** – numeric coefficient extraction via evaluation at x=0,1,2, then use the quadratic formula. Complex roots are supported.  
- **Polynomial simplifier** – expands brackets, combines like terms, normalises to `ax² + bx + c`. Used only when needed.  

### 2.4 Natural Language (`NaturalLanguage.cpp`)

- Simple regex?based replacement of fraction words (`half` ? `1/2`), operator words (`of` ? `*`, `into` ? `*` with swapped operands, `by` ? `/`), and `percent` (e.g., `5 percent` ? `5%`).  
- Called early in `MathEngine::tryArithmetic`.

### 2.5 Big Numbers (`BigNum.cpp` / `BigNum.h`)

- Uses `boost::multiprecision::cpp_int` and `cpp_dec_float<50>`.  
- `bigFactorial` – includes a progress callback and cancellation flag.  
- `bigEval` – evaluates expressions using `BigDec` for high precision.  
- Exposed through `MathEngine::bigEval` and `MathEngine::bigFactorial`.

---

## 3. Threading & Job Processing

### 3.1 Persistent Worker (`PersistentWorker`)

- Lives in a dedicated `QThread`.  
- Maintains a queue of `CalcJob` (id + expression).  
- Processing loop:  
  1. Dequeue a job, reset cancellation flag.  
  2. If the job matches a standalone factorial pattern, call `BigNum::bigFactorial` with a progress callback that emits `progress(int, int, QString)`.  
  3. Otherwise, call `MathEngine::evaluate`.  
  4. Emit `resultReady(id, result, type, formula)`.  
- Cancellation – `cancelAll()` clears the queue and sets an atomic flag; the worker thread checks this flag before each job and also inside `bigFactorial`.  

### 3.2 MainWindow Integration

- `run(expr)` submits a job, stores the expression in `m_pendingJobs`, and sets `RunState::Running`.  
- `onWorkerFinish` receives the result, calls `OutputArea::addResultLine`, updates the session panel, and resets state to `Idle`.  
- The stop button calls `cancelAll()` on the worker and also cancels any active shape prompt.  

---

## 4. User Interface

### 4.1 MainWindow

- Central stacked widget – page 0: terminal + sidebar; page 1: geometry mode.  
- Header: CRT?styled logo + mode buttons.  
- Input line: `DraggableExpressionEdit` (allows dragging constants).  
- Sidebar: `SidebarPanel` with clickable/double?clickable examples.  
- Session panel: shows calculation count, last result (first line only), last expression.  

### 4.2 OutputArea

- **`OutputWidget`** – used for arithmetic, algebra, trig, error results.  
  - Left?click copies the result (`m_copyText`).  
  - Right?click? Actually we use left?click for copy, double?click for formula (conversions).  
  - For arithmetic results, also supports **dragging** to adjust constants (see BUG?001).  
- **`ConversionLabel`** – for unit conversion results; double?click toggles formula display.  
- **`ClickableLabel`** – used for big numbers (with manual wrapping) and errors.  
- All result lines have a left padding of 22px and a fade?in animation.  

### 4.3 Geometry Cards (`GeoCard` hierarchy)

- Each shape (circle, triangle, cube, etc.) derives from `GeoCard`.  
- `addSlider()` and `addResult()` add UI elements.  
- Sliders trigger `recompute()`, which updates the result rows and (for 3D cards) the renderer.  
- **Copy Card** button builds a text summary with parameters and results.  
- **Show Shape Projection** button emits a signal that switches the stacked widget to page 1 and passes the shape type + current parameters to `GeoModeWidget`.

### 4.4 Geometry Mode (`GeoModeWidget` + `RenderWidget` + `OpenGLRenderer`)

- **Full?window** – the renderer covers the whole widget; control panel (sliders, colour pickers) and back button are overlays (children of the render widget).  
- **`RenderWidget`** – selects between Vulkan (stub) and OpenGL; forwards shape?setting and colour commands.  
- **`OpenGLRenderer`** – draws wireframe shapes (cube, sphere, cylinder, cone, torus) using immediate mode.  
  - Supports mouse drag to rotate (when auto?rotation is off).  
  - Supports shape colour and background colour (grayscale) via sliders.  
  - A combo box in the top overlay allows shape switching, which rebuilds the parameter sliders and updates the renderer.  
- **`RenderShape`** hierarchy – each shape (Cube, Sphere, etc.) implements `draw()` and `setParameters()`.  

### 4.5 Input Handling

- **`InputRouter`** – translates key events into `InputAction` (Submit, HistoryBack, etc.).  
- **`HistoryNavigator`** – stateless, reads history list by const reference, provides back/forward navigation.  
- **`PromptController`** – manages interactive shape parameter prompts (e.g., collecting a, b, c for a triangle). It updates the input placeholder and a temporary label.  
- **`DraggableExpressionEdit`** – a `QLineEdit` subclass that allows clicking and dragging on numeric constants to adjust them, with live re-evaluation.

### 4.6 Animations

- `Animations` namespace provides static functions:  
  - `fadeIn` – uses `QGraphicsOpacityEffect` and `QPropertyAnimation`.  
  - `flash` – temporarily changes button background colour.  
- Applied to result lines, buttons, and window startup.

---

## 5. Data Flow – From User Input to Output

1. **User types** in `DraggableExpressionEdit` and presses Enter (or clicks RUN).  
2. `MainWindow::onRun()` calls `submitExpression()`, which:
   - Adds the input line to `OutputArea`.
   - Updates the session panel.
   - Calls `run(expr)`, which submits a job to `PersistentWorker`.
3. **Worker thread** processes the job:
   - If expression is a standalone factorial, it calls `BigNum::bigFactorial` with a callback for progress.
   - Otherwise, calls `MathEngine::evaluate(expr)`.
4. `MathEngine::evaluate` dispatches:
   - `tryConversion` (if `to` present) ? unit conversion.
   - `tryGeometry` (if shape name) ? returns `"geo"` type.
   - `tryTrig` ? trigonometric evaluation.
   - `tryAlgebra` ? equation solving.
   - `tryArithmetic` ? numeric evaluation (including natural language).
5. **Result** is returned as `CalcResult` (result string, type, optional formula).  
6. Worker emits `resultReady(id, result, type, formula)`.  
7. `MainWindow::onWorkerFinish`:
   - If `type == "geo"`, creates a `GeoCard` (on UI thread) and adds it to `OutputArea`.
   - If `type == "big"`, truncates the result (first 500 chars) and adds it.
   - Otherwise, calls `OutputArea::addResultLine` with the original expression, result, type, and formula.
8. `OutputArea::addResultLine` creates the appropriate widget (`OutputWidget`, `ConversionLabel`, or `ClickableLabel`) and inserts it into the scroll area.  
9. The widget handles copy/drag/double?click interactions as described above.  

---

## 6. Important Signal?Slot Connections

| Sender                 | Signal                              | Receiver (slot)                     | Purpose                               |
|------------------------|-------------------------------------|-------------------------------------|---------------------------------------|
| `m_worker`             | `resultReady(int, QString, QString, QString)` | `MainWindow::onWorkerFinish` | Receive calculation results.          |
| `m_worker`             | `progress(int, int, QString)`       | `MainWindow` lambda                 | Update progress bar.                  |
| `MainWindow::stop`     | `stop()`                            | `PersistentWorker::cancelAll`       | Cancel current calculation.           |
| `m_output`             | `shapeProjectionRequested`          | `MainWindow::onShowGeometryMode`    | Switch to geometry mode.              |
| `m_geoModeWidget`      | `backClicked`                       | `MainWindow` lambda                 | Return to calculator mode.            |
| `m_promptCtrl`         | `paramReady` / `promptComplete` / `promptCancelled` | `MainWindow` slots | Shape prompting flow.                 |

---

## 7. Known Limitations & Future Work

- **No persistent variables** – each expression is evaluated in isolation.  
- **3D shapes** – wireframe only; no lighting or solid shading.  
- **Graph plotting** – not implemented.  
- **History dock** – partially coded but commented out.  
- **Algebra** – only linear and quadratic; no cubic or higher.  
- **Expression parser** – factorial limit of 20 inside expressions (e.g., `(5+3)!` works only for integers >20).  

See the **DEV_NOTES.md** bug list for open issues.

---

*Last updated: 2026-06-15*