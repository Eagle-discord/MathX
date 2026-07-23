# MATHX � Architecture & Implementation Notes

This document describes the internal design of MATHX, a multi-threaded calculator with support for arithmetic, algebra, unit conversion, interactive geometry, and a 3D shape viewer. It is intended for maintainers and as a reference for future development.
# DEV_NOTES.md � Known Issues & Future Work

This document tracks known bugs (with reproduction steps and fix theories) and planned features for MATHX.  
Last updated: 2026-07-15

---

## ?? BUG-001 � Draggable Result Labels: number replacement fails on multiple drags  
**Date found:** 2025-06-07  
**Recreation:**  
1. Enter `2+2` ? result `4`.  
2. Drag the result `4` to the right ? number increases, expression updates to `2+4` ? result `6`.  
3. Drag again the new result `6` ? the number in the expression is replaced incorrectly (appending instead of replacing, e.g., `2+46`).  

**Theory for fix:**  
The stored original expression and the number substring are not updated after the first drag. In `OutputWidget::rebuildExpression`, we must update `m_originalExpr` and `m_currentNumber` to the new values after re?evaluation. Ensure the drag starts with the current displayed number (which is the result, not the original operand). A clean solution is to treat the result as the only draggable quantity and replace it directly in the original expression (which works only if the original expression is a single number). Better to implement `DraggableResultLabel` as a separate class that works only for pure arithmetic expressions where the result equals the number in the expression.

---

## ?? BUG-002 � Mouse rotation direction feels inverted when auto-rotation is off  
**Date found:** 2025-06-08  
**Recreation:**  
1. Disable auto?rotation (checkbox unchecked).  
2. Click and drag the shape horizontally ? shape rotates opposite to mouse movement.  

**Theory for fix:**  
The sign in `mouseMoveEvent` was incorrect. Correct mapping:  
`m_yRot += delta.x() * 0.5f;` (right ? rotate right)  
`m_xRot -= delta.y() * 0.5f;` (down ? rotate down).  

---

## ?? BUG-003 � Prompt handles `b = a` incorrectly (does not evaluate previous parameter)  
**Date found:** 2025-06-05  
**Recreation:**  
1. Enter `tri a = 34 b = a`.  
2. After setting `a=34`, the prompt asks for `b`. Typing `a` is rejected.  

**Theory for fix:**  
The prompt input evaluation should use the existing parameter values (stored in `m_promptVars`) when evaluating expressions. Modify `handlePromptInput` to first try `MathEngine::evalWith(value, m_promptVars, ok)`. This will resolve `a` to its current value. The code now has a `VarMap m_promptVars` and uses it in `handlePromptInput` � verify it works.

---

## BUG-003b � Settings page does not apply changes correctly (maybe)  
**Date found:** 2026-06-15  
**Recreation:** (To be verified)  
1. Open Settings page.  
2. Change a setting (e.g., font size).  
3. Navigate away � does the change apply?  

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

- **Persistent variables** � allow `x = 5` and reuse `x` in later expressions.  
- **Graph plotting** � plot functions like `y = x^2` using a separate widget.  
- **History dock** � store previous expressions with double?click to re?evaluate.  
- **3D lighting & solid shading** � replace wireframe with lit, solid faces.  
- **Export geometry mode screenshot** � save current 3D view as PNG.  
- **Settings import/export** � backup and restore user preferences.  
- **Multi?language support** � i18n via Qt `.ts` files.  
- **Theming (light/dark)** � full light theme support (currently dark only).  
- **Undo/redo** � for expression input and shape parameter changes.  

### Formula Walkthroughs & MathText (pending, added 2026-07-11)

Current state: clicking a formula on the geometry-mode properties card (formula glows on hover)
plays a walkthrough for the **cuboid Surface Area** only: l/w/h edges highlight colour-coded while
CMU-Serif letters + `= value` annotations write themselves on via `MathText` (`src/render/MathText.*`,
glyph outlines harvested from the font; layout via `Line`; draw-on via `draw()`).
`OpenGLRenderer::startFormulaAnimation` no-ops for everything else. Pending:

- **Params-expand-on-click** � indicator shows only the letter (`w`) by default; clicking it
  smoothly writes out the `= value` annotation (second `MathText::draw` with its own `t`).
- ~~**Face-pair derivation phase (SA)**~~ � DONE 2026-07-12 (final form): dimensions draw
  simultaneously; each term builds ONE face ("Term: lw" headline, border trace + fill wipe +
  area caption) and PERSISTS; then the "2" flares in the readout and the tinted half-shell
  duplicates as ONE rigid block (yaw+flip = point reflection) that swings around the box onto
  the opposite three faces with a fast-fading bloom burst, completing the surface.
- **Volume walkthrough (cuboid)** � l�w�h visualised (e.g. filling slabs along one axis).
- **Walkthroughs for other shapes** � cube, cylinder, sphere, ...: per-shape indicator/edge tables
  in `rebuildIndicatorGeometry` + formula-key dispatch in `startFormulaAnimation`.
- **MathText layout extensions** � fractions, radicals, sub/superscript groups, integral bounds;
  needed for Diagonal `√(l²+w²+h²)` and full poster-style compositions.
- **Terminal geo-card formula clicks** � cards in the output area share `ResultRow` (hover glow
  already appears) but clicking does nothing; should jump to geometry mode AND play the walkthrough.
- **Label collision avoidance** � with free rotation, an edge label can drift behind the shape or
  brush the top readout; consider gentle repulsion from the readout band / other labels.
- **Glow controls** � expose bloom intensity/exposure (currently `m_glowIntensity = 0`, floor glow
  only) as sliders in the geometry control panel.

---

## SESSION LOG - 2026-07-15 (regression suite + perf hunt)

A 380-line regression suite was built and run against batch mode. Every test that
reduces to a number is written as an equality (`2+2 = 4`), so `tryAlgebra`'s
no-variable path prints `LHS = RHS  OK` or `LHS != RHS  X` -- the existing
equality-verification feature IS the assertion engine. Scan the output for `X`.
Suite file: `tests/mathx_regression.txt` (255 assertions, 127 eyeball probes).

Everything below was found by that suite or by reading code during the hunt.
Root causes are confirmed with file:line unless marked THEORY.

---

### BUG-005 - lcm() returns 0 for every input -- FIXED 2026-07-16
**Found:** 2026-07-15 (regression suite)
**Recreation:** `lcm(4,6)` -> 0. `lcm(3,5)` -> 0. All inputs.
**Root cause:** `Expr.cpp:174-185`. The Euclid loop mutates `b` down to 0, then
line 182 uses that dead `b` as the multiplicand:
```
long long a = result;
while (b) { a %= b; std::swap(a, b); }   // <- destroys b
result = result / a * b;                  // <- b is now 0
```
**Fix:** save the original `b` before the loop, multiply by the saved value.
Also guard `lcm(0,0)` (gcd returns 0 -> division by zero).

---

### BUG-006 - logbase() arguments reversed -- FIXED 2026-07-16
**Found:** 2026-07-15 (regression suite)
**Recreation:** `logbase(8,2)` -> 0.3333 (expected 3).
**Root cause:** `Expr.cpp:188` - `std::log(args[1]) / std::log(args[0])`.
**Fix:** swap to `args[0] / args[1]`.

---

### BUG-007 - __BIGFACT__ sentinel is thrown but never caught -- FIXED 2026-07-16
**Found:** 2026-07-15 (regression suite)
**Recreation:** `fact(5) = 120` -> "Cannot parse expression". Same for `21!`, `25!`.
**Root cause:** `Expr.cpp:215` throws `runtime_error("__BIGFACT__:" + n)`.
`grep -rn "__BIGFACT__"` returns exactly ONE hit - the throw. No handler exists
anywhere in the tree. Standalone `fact(5)` only works because `dispatchEvaluate`'s
`factRe` intercepts it before `Expr` ever runs; adding `= 120` stops that regex
matching and the sentinel escapes as a parse error.
**Fix:** catch the sentinel in `evalWith`/`callBuiltin`'s caller and route to
`BigNum::bigFactorial`, or drop the sentinel and dispatch directly.

---

### BUG-008 - '%' is both binary mod and postfix percent; postfix always wins
**Found:** 2026-07-15 (regression suite)
**Recreation:** `10%3` -> "Cannot parse expression".
**Root cause:** `applyPostfix` (`Expr.cpp:445`) consumes `%` immediately after a
number and divides by 100, so `10%` becomes 0.1 and the following `3` is an
unexpected token. The binary Mod branch at `Expr.cpp:395` is unreachable for
`N%N`. Genuinely ambiguous grammar - needs a rule, not just a patch.
**Suggested rule:** only treat `%` as percent when the NEXT token is not a
number / identifier / '('.

---

### BUG-009 - Four settings keys are read but never registered -> setPointSize(0)
**Found:** 2026-07-15 (debug log analysis)
**Symptom:** `QFont::setPointSize: Point size <= 0 (0)` twice per result line,
forever. Plus `CacheOverflowException` x5 at startup. Every tracked widget falls
back to a system font instead of the loaded one.
**Root cause:** `Settings.cpp:357-362` reads these keys:
```
appearance/typography/fontSizeUI
appearance/typography/fontSizeResults
appearance/typography/fontSizeSeparators
appearance/typography/fontSizeInput
appearance/typography/fontSizeSidebar
```
`SettingsDef.cpp` registers only `fontSize`, `fontFamily`, `fontWeight`, and
`fontSizeSplash`. `get()` on an unregistered key returns an invalid QVariant;
`QVariant().toInt()` is 0. `WidgetRegistry::syncFromSettings()` (lines 16-21)
then OVERWRITES the sane in-class defaults (9, 10) with 0, and `buildFont()`
(`WidgetRegistry.cpp:291`) calls `setPointSize(0)` on every widget from then on.
**Fix:** (1) register the five missing keys in `SettingsDef.cpp` next to
`fontSizeSplash`; (2) belt-and-braces at `WidgetRegistry.cpp:291`:
`f.setPointSize(pt > 0 ? pt : 9);`
**Prevention:** a read-without-registration is silently 0. Consider asserting in
`get()` that the key exists.

---

### BUG-010 - CRTTextLabel flicker cannot be turned off -- WORKED AROUND 2026-07-16
**Found:** 2026-07-15 (perf hunt)
**Symptom:** logo repaints 12.5x/sec for the entire life of the app, saturating
the event loop. THIS WAS THE UI BOTTLENECK - confirmed by commenting out the
timer and re-running the batch.
**Root cause:** `MainWindow.cpp:610` has `logo->setFlickerEnabled(true);`
commented out, but `CRTTextLabel`'s constructor calls `m_flickerTimer->start(80)`
UNCONDITIONALLY. `m_flickerEnabled` defaults to true. The off-switch is bypassed.
**Fix:** `if (m_flickerEnabled) m_flickerTimer->start(80);` in the constructor.
**2026-07-16:** the timer creation, connect, AND start are now all commented
out in the constructor. That fixes the perf issue but leaves a landmine:
`setFlickerEnabled(true)` now calls `m_flickerTimer->start()` on a NULL
pointer -> crash. Nothing calls it today. Either guard the setter or restore
the timer with the conditional start.

---

### BUG-011 - CRTTextLabel::paintEvent runs three full-image pixel passes per frame
**Found:** 2026-07-15 (perf hunt)
**Root cause:** the inline pixel loop after `p.end()` in `paintEvent` is a
hand-rolled DUPLICATE of `stripTargetColors()`, which already runs twice (once
from `drawScanlines`, once from `drawVignette`). The inline copy also does
`for (QColor targetColor : targets)` - by value - constructing 5 QColors per
pixel, plus a `QColor current(line[x])` per pixel.
`drawSubtitle` constructs a `QFontMetrics` TWICE PER CHARACTER inside its loop.
**Fix:** delete the inline loop; `const QColor&` in the range-for; hoist
`QFontMetrics` out of the subtitle loop; cache the composed QImage and only
re-composite alpha on flicker tick (scanlines/vignette never change).

---

### BUG-012 - bindLivePair never connects; drag-to-recompute is dead app-wide
**Found:** 2026-07-15 (debug log)
**Symptom:** `[scrub] nothing is connected to expressionEdited for "..."` fires
for EVERY result line. Our own diagnostic caught it.
**Theory:** `addResultLine` does call `bindLivePair`, but `m_pendingInput` is
already null by then. Something clears or never sets it. Needs tracing.

---

### BUG-013 - Batch mode blocks the event loop for the entire run
**Found:** 2026-07-15 (code read)
**Root cause:** `BatchRunner::onJobDone()` with delay=0 (the default) calls
`runNext()` SYNCHRONOUSLY, so `runNext -> emit -> ... -> onJobDone -> runNext`
recurses once per expression and never returns to the event loop. Consequences:
no repaints during a batch, Stop button dead, the 30s timeout timer cannot fire,
queued `scrollToBottom` timers all fire in a burst at the end, and the stack goes
N frames deep.
**Fix:** `QTimer::singleShot(0, this, [this]{ if (m_state==State::Running) runNext(); });`

---

### BUG-014 - Unicode multiplication sign silently becomes an equation -- FIXED
**Found:** 2026-07-15 (regression suite)
**Recreation:** `10 x 10 = 100` (with U+00D7) -> `x = 1`.
**Root cause:** `NaturalLanguage.cpp` maps U+00D7 and U+2715 to `" x "`, betting
on `replaceXWithTimes` firing later. It doesn't - `tryAlgebra` grabs the `=`
first and solves for x. Silently wrong answer.
**Fix:** map to `" * "` instead. Unambiguous.

---

### BUG-015 - Ordinal / fraction collision in NaturalLanguage
**Found:** 2026-07-15 (regression suite)
**Recreation:** `a third of 90` -> 270 (expected 30). Same for `a fourth of 80`
-> 320, `an eighth of 80` -> 640, `a fifth of 100` -> 500, `a tenth of 100` -> 1000.
**Root cause:** `expandNumberWords`'s `wordMap` maps the ordinals
(`third`->3, `fourth`->4, ...) to digits BEFORE `lateFractionPatterns` can map
them to fractions. The comment above `lateFractionPatterns` claims the opposite.
Those patterns are effectively unreachable for every ordinal.
**Fix (decision needed):** either drop the ordinals from `wordMap`, or move the
late fraction patterns before `expandNumberWords`. Former is probably right -
what is "the third" as a number-word for?

---

### BUG-016 - Fraction literals lose precision -- FIXED
**Found:** 2026-07-15 (regression suite)
**Recreation:** `two thirds of 90` -> 59.99999999. `one third of 90` -> both
sides print 30 but compare unequal (29.999999997 displayed as 30).
**Root cause:** both fraction tables use 10-digit decimal literals
(`(0.3333333333)`) instead of exact fractions.
**Fix:** replace every decimal with a fraction - `(1/3)`, `(2/3)`, `(1/4)`, etc.
BigDec evaluates them exactly.

---

### BUG-017 - "dozen" replaced without word boundary -- FIXED
**Found:** 2026-07-15 (regression suite)
**Recreation:** `dozenth` -> "'th' is not defined".
**Root cause:** `result.replace("dozen", "(12)")` is a plain substring replace.
**Fix:** `QRegularExpression(R"(\bdozens?\b)")`.

---

### BUG-018 - "multiply X by Y" divides
**Found:** 2026-07-15 (regression suite)
**Recreation:** `multiply 6 by 4` -> "Unknown function: multiply".
**Root cause:** the multi-word pass only handles "multiplied by", not "multiply
... by", so the bare `byRe` in the fixpoint loop fires and maps `by` -> `/`.
**Note:** the fixpoint loop (`ofRe`/`timesRe`/`plusRe`/`byRe`/`intoRe`, all using
`[^\s]+`) is the most fragile code in NaturalLanguage.cpp. It only terminates
because each pass removes a keyword; the intermediate strings are arbitrary. Do
not touch it without running the suite.

---

### BUG-019 - RETRACTED 2026-07-16: `where` works; the TEST was malformed
**Original claim:** "EXPR where x = 5" is completely non-functional.
**Reality:** the suite wrote each case as an equality (`x^x where x = 5 = 3125`)
so it could self-check. That breaks the binding. `tryWhere`'s bindRe is
`^([A-Za-z][A-Za-z0-9_]*)\s*=\s*(.+)$` -- the greedy `(.+)` captures
"5 = 3125" as the VALUE, `bigEvalValue` rejects it, and tryWhere declines at
`MathEngine.cpp:1280`. Everything then falls through to "Undefined: where, x".
`tryWhere` itself is correct. Suite section 19 moved to the eyeball part.
**Worth noting:** a greedy `(.+)` for the binding value means `where x = 5 = 3`
can never report a useful error -- it just declines. Low priority, but if the
`where` syntax ever gets stricter, that's the line.

---

### BUG-020 - Unbalanced paren silently accepted -- FIXED 2026-07-16
**Recreation:** `(2+3` -> 5. Expected an unbalanced-paren error.
**Root cause (the earlier theory was wrong -- it was never Expr):**
`BigNum.cpp:230`, in `bPrim()`:
```
if (t.peek() == '(') { t.get(); BigDec v = bExpr(t); t.skip();
    if (t.peek() == ')') t.get();      // <- shrugs if ')' is missing
```
Expr's parser DOES throw correctly (`consume(RParen)`), but `tryArithmetic`
calls `BigNum::bigEvalValue` FIRST. The lenient parser wins and returns 5 with
ok=true, so the strict one never runs. Note `bCallFn` at `BigNum.cpp:117`
already throws "Missing ')'" -- BigNum was inconsistent with itself.
**Fix applied:** `if (t.peek() != ')') throw std::runtime_error("Missing ')'");`
**Lesson:** two parsers with different strictness, and the permissive one runs
first. Any future Expr-level validation is dead code unless BigNum agrees.

---

### BUG-021 - 1 liter to ml returns 1 ml
**Found:** 2026-07-15 (regression suite)
**Recreation:** `1 liter to ml` -> `1 liter = 1 ml`. Expected 1000.
**Also:** `1 acre to m2`, `1 kPa to Pa`, `km/h to m/s` (no leading number), and
`ft/s to mph` all return "Undefined" despite the README advertising full SI
prefix support and 30+ categories.

---

### BUG-022 - Three competing unit tables
**Found:** 2026-07-15 (regression suite)
**Symptom:** `1 parsec` -> 30856775815000000 (real DB) but tryArithmetic's own
table says 3.086e16. `1 furlong` -> "201.168 m" (real DB) while
`3 furlongs in meters` -> "603.504 meters" (shadow table). Two spellings, two
tables, same session.
**Root cause:** `tryArithmetic` has a hardcoded `static const QVector<UnitDef>
units` (furlong, parsec, km, feet, ...) that duplicates and shadows
`tryConversion.cpp`'s 200+ unit database. `km` and `feet` appear in both.
**Fix:** delete the shadow table; make tryConversion own all units.

---

### BUG-023 - Equality results render with double line spacing
**Found:** 2026-07-15 (screenshot)
**Symptom:** the LHS/RHS proof renders ~7 rows for 4 lines of text. Roughly
doubles the height and text-layout cost of the most common result type.
**Theory:** `buildResultHtml` emits `<span>...</span><br>` per line, and the
conv/else branches then run `html.replace("\n", "<br>")` over text that already
has `<br>`s. Needs tracing.

---

### BUG-024 - Session panel shows the first line of a multi-line result
**Found:** 2026-07-15 (screenshot)
**Symptom:** `Last result: LHS (Left-hand Side).` Cosmetic.

---

### PERFORMANCE - what was measured, and what turned out to be false

The UI slowness was real and had exactly ONE cause: BUG-010 (the flicker timer).
Everything else was ruled out by measurement. Recording the negatives so nobody
re-chases them:

| Theory | Verdict | Evidence |
|--------|---------|----------|
| Math engine is slow | FALSE | 0.5-0.9 ms/expression; 380 lines is <1s of compute |
| `addResultLine` is slow | FALSE | Flat 2.5 ms across 8 lines, no growth |
| Fade-in animations | FALSE | Removing them changed nothing |
| Leaked QGraphicsOpacityEffect | FALSE | Fixed the leak; no improvement |
| O(n^2) layout / sizeHint | FALSE | Timings flat; run 5 as fast as run 1 |
| Need to virtualize the output area | FALSE | 271 widgets, zero degradation |
| Widget accumulation / leak | FALSE | +22 per 5-line run, dead linear, no leak |
| Debug build overhead | Minor | Release confirmed same flat profile |

Release, five consecutive 5-line batches, 271 widgets at the end:
every line 0.5-1.0 ms, run 5 identical to run 1. Memory +2 MB/run, linear.

The only outlier is the FIRST expression of a session (~43 ms) - cold start of
the font cache, first widget construction, first regex compile. Appears once.
Could be warmed at startup like `NaturalLanguage::warmUp()` already is.

**Lesson:** the widget counter in the session panel was the instrument that
settled this, and it existed the whole time. Measure before optimizing.

---
## Recently Fixed

All five original bugs (BUG-001 .. BUG-004, BUG-003b) were fixed but never
recorded. Closed out 2026-07-15. If any of these recur, the entry below is the
starting point -- the *theory* is preserved above; whether the theory matched
the actual fix was not recorded at the time, which is exactly the gap this
section exists to close.

- BUG-001 - Draggable result labels: multi-drag replacement. FIXED (date unrecorded).
- BUG-002 - Inverted mouse rotation with auto-rotate off. FIXED (date unrecorded).
- BUG-003 - Prompt `b = a` not evaluating previous params. FIXED (date unrecorded).
- BUG-003b - Settings page apply pipeline. FIXED (date unrecorded).
- BUG-004 - RegistryWatcher startup freeze (parent + moveToThread). FIXED (date unrecorded).

**Process note:** the reason five fixes went unrecorded is that nothing forces
a write on the way out. The footer already says to tag commits `--bug BUG-XXX`;
if that is being done, the git log is the ground truth and this section can be
reconciled mechanically. If not, that is the leak.

---




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

## 2. Math Engine � Core Evaluation

### 2.1 Expression Parser (`Expr.cpp` / `Expr.h`)

- **Tokeniser** � reads the input string, recognises numbers, identifiers, operators, parentheses, postfix `!` and `%`.  
- **Recursive descent parser** � implements precedence (`+`, `-` lowest, then `*`, `/`, `%`, then `^`, then unary `+`/`-`, then postfix operators, then primary).  
- **Variables** � passed via a `VarMap` (QMap<QString, double>).  
- **Functions** � dispatched to `callBuiltin`, which contains all mathematical functions (trig, hyperbolic, log, factorial, combinatorics, etc.).  
- **Error handling** � unknown identifiers now throw an exception (instead of returning `0`). The exception is caught in `evalWith` and sets `ok = false`.

**Preprocessing steps** (in `evalWith`):
- Normalise brackets (`[`?`(`, etc.)
- Replace `**` with `^`
- Replace `x` as multiplication (`replaceXWithTimes`)
- Insert implicit multiplication (`insertImplicitMul`) � e.g., `2(3+4)` ? `2*(3+4)`

### 2.2 Unit Conversion (`tryConversion.cpp`)

- **Unit database** � built once into a `QMap<QString, ResolvedUnit>` from three tables: `SI_BASES` (prefixable), `MANUAL` (non?SI), `NONLINEAR` (temperature). Prefixes are automatically generated for SI bases.  
- **Resolution** � a unit expression may be simple (`"km"`) or compound (`"km/h"`). The compound parser splits numerator/denominator lists and computes the combined factor.  
- **Temperature** � uses explicit formulas (e.g., `�F = (�C � 9/5) + 32`) because linear factors are not meaningful.  
- **Raw conversion** � `100 km` alone converts to a default unit (e.g., `m`).  
- **Mach special case** � `mach 1` is rewritten to `1 mach` before parsing.  

### 2.3 Equation Solving (`Algebra.cpp` + `Solver.cpp`)

- **Linear equations** � fast method: evaluate at x=0,1,2 to obtain coefficients `a` and `b`.  
- **Quadratic equations** � numeric coefficient extraction via evaluation at x=0,1,2, then use the quadratic formula. Complex roots are supported.  
- **Polynomial simplifier** � expands brackets, combines like terms, normalises to `ax� + bx + c`. Used only when needed.  

### 2.4 Natural Language (`NaturalLanguage.cpp`)

- Simple regex?based replacement of fraction words (`half` ? `1/2`), operator words (`of` ? `*`, `into` ? `*` with swapped operands, `by` ? `/`), and `percent` (e.g., `5 percent` ? `5%`).  
- Called early in `MathEngine::tryArithmetic`.

### 2.5 Big Numbers (`BigNum.cpp` / `BigNum.h`)

- Uses `boost::multiprecision::cpp_int` and `cpp_dec_float<50>`.  
- `bigFactorial` � includes a progress callback and cancellation flag.  
- `bigEval` � evaluates expressions using `BigDec` for high precision.  
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
- Cancellation � `cancelAll()` clears the queue and sets an atomic flag; the worker thread checks this flag before each job and also inside `bigFactorial`.  

### 3.2 MainWindow Integration

- `run(expr)` submits a job, stores the expression in `m_pendingJobs`, and sets `RunState::Running`.  
- `onWorkerFinish` receives the result, calls `OutputArea::addResultLine`, updates the session panel, and resets state to `Idle`.  
- The stop button calls `cancelAll()` on the worker and also cancels any active shape prompt.  

---

## 4. User Interface

### 4.1 MainWindow

- Central stacked widget � page 0: terminal + sidebar; page 1: geometry mode.  
- Header: CRT?styled logo + mode buttons.  
- Input line: `DraggableExpressionEdit` (allows dragging constants).  
- Sidebar: `SidebarPanel` with clickable/double?clickable examples.  
- Session panel: shows calculation count, last result (first line only), last expression.  

### 4.2 OutputArea

- **`OutputWidget`** � used for arithmetic, algebra, trig, error results.  
  - Left?click copies the result (`m_copyText`).  
  - Right?click? Actually we use left?click for copy, double?click for formula (conversions).  
  - For arithmetic results, also supports **dragging** to adjust constants (see BUG?001).  
- **`ConversionLabel`** � for unit conversion results; double?click toggles formula display.  
- **`ClickableLabel`** � used for big numbers (with manual wrapping) and errors.  
- All result lines have a left padding of 22px and a fade?in animation.  

### 4.3 Geometry Cards (`GeoCard` hierarchy)

- Each shape (circle, triangle, cube, etc.) derives from `GeoCard`.  
- `addSlider()` and `addResult()` add UI elements.  
- Sliders trigger `recompute()`, which updates the result rows and (for 3D cards) the renderer.  
- **Copy Card** button builds a text summary with parameters and results.  
- **Show Shape Projection** button emits a signal that switches the stacked widget to page 1 and passes the shape type + current parameters to `GeoModeWidget`.

### 4.4 Geometry Mode (`GeoModeWidget` + `RenderWidget` + `OpenGLRenderer`)

- **Full?window** � the renderer covers the whole widget; control panel (sliders, colour pickers) and back button are overlays (children of the render widget).  
- **`RenderWidget`** � selects between Vulkan (stub) and OpenGL; forwards shape?setting and colour commands.  
- **`OpenGLRenderer`** � draws wireframe shapes (cube, sphere, cylinder, cone, torus) using immediate mode.  
  - Supports mouse drag to rotate (when auto?rotation is off).  
  - Supports shape colour and background colour (grayscale) via sliders.  
  - A combo box in the top overlay allows shape switching, which rebuilds the parameter sliders and updates the renderer.  
- **`RenderShape`** hierarchy � each shape (Cube, Sphere, etc.) implements `draw()` and `setParameters()`.  

### 4.5 Input Handling

- **`InputRouter`** � translates key events into `InputAction` (Submit, HistoryBack, etc.).  
- **`HistoryNavigator`** � stateless, reads history list by const reference, provides back/forward navigation.  
- **`PromptController`** � manages interactive shape parameter prompts (e.g., collecting a, b, c for a triangle). It updates the input placeholder and a temporary label.  
- **`DraggableExpressionEdit`** � a `QLineEdit` subclass that allows clicking and dragging on numeric constants to adjust them, with live re-evaluation.

### 4.6 Animations

- `Animations` namespace provides static functions:  
  - `fadeIn` � uses `QGraphicsOpacityEffect` and `QPropertyAnimation`.  
  - `flash` � temporarily changes button background colour.  
- Applied to result lines, buttons, and window startup.

---

## 5. Data Flow � From User Input to Output

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

- **No persistent variables** � each expression is evaluated in isolation.  
- **3D shapes** � wireframe only; no lighting or solid shading.  
- **Graph plotting** � not implemented.  
- **History dock** � partially coded but commented out.  
- **Algebra** � only linear and quadratic; no cubic or higher.  
- **Expression parser** � factorial limit of 20 inside expressions (e.g., `(5+3)!` works only for integers >20).  

See the **DEV_NOTES.md** bug list for open issues.

---

### Current Objective
* Take a break from new features, and fix all existing bugs instead


*Last updated: 2026-07-15*