#pragma once
#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include "InputAction.h"

// -- InputRouter ---------------------------------------------------------------
// Sole responsibility: translate a QKeyEvent into an InputAction.
//
// Owns NO state. Takes context flags as parameters so callers decide
// what is valid in the current situation. This means the router never
// needs to know about prompts, history, or run state - it only maps keys.
//
// Usage:
//   InputAction action = InputRouter::translate(ke, promptActive, historyEmpty);
//   switch (action) { ... }
// -----------------------------------------------------------------------------
class InputRouter {
public:
    // Translate a key event into a semantic action given the current context.
    //   promptActive  - true when a shape parameter prompt is in progress
    //   historyEmpty  - true when there are no history entries to navigate
    //   ghostVisible  - true when an inline completion is currently offered
    static InputAction translate(const QKeyEvent* ke,
        bool promptActive,
        bool historyEmpty,
        bool ghostVisible = false)
    {
        if (!ke) return InputAction::None;

        switch (ke->key()) {

        case Qt::Key_Return:
        case Qt::Key_Enter:
            return InputAction::Submit;

        case Qt::Key_Tab:
            // Only claim Tab when there's actually something to accept.
            // Otherwise it must fall through to Qt so focus traversal still
            // works -- stealing Tab unconditionally would trap keyboard users in
            // the input field with no way out.
            return ghostVisible ? InputAction::Complete : InputAction::None;

        case Qt::Key_Escape:
            // Escape only means anything when a prompt is active
            return promptActive ? InputAction::CancelPrompt : InputAction::None;

        case Qt::Key_Up:
            // History navigation suppressed during prompt
            if (promptActive || historyEmpty) return InputAction::None;
            return InputAction::HistoryBack;

        case Qt::Key_Down:
            // History navigation suppressed during prompt
            if (promptActive || historyEmpty) return InputAction::None;
            return InputAction::HistoryForward;

        default:
            return InputAction::None;
        }
    }
};