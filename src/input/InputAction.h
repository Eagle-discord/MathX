#pragma once

// All discrete actions the input system can produce from a key event.
// InputRouter translates raw QKeyEvent → InputAction.
// MainWindow reacts to actions — it never inspects key codes directly.
enum class InputAction {
    None,           // key has no special meaning in current context
    Submit,         // Enter / Return — run current text
    HistoryBack,    // Up arrow — go to older history entry
    HistoryForward, // Down arrow — go to newer history entry
    CancelPrompt,   // Escape — abort active shape prompt
    Complete,       // Tab — accept the inline ghost suggestion
};