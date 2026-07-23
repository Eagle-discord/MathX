#pragma once
#include <QString>
#include <QStringList>

// -- HistoryNavigator ----------------------------------------------------------
// Owns exactly two pieces of state:
//   - the current navigation index (0 = not navigating)
//   - the text the user was typing before they started navigating
//
// Reads the history list by const reference - never mutates it.
// The QLineEdit is NOT touched here; callers read currentText() and
// set it themselves. This keeps Qt UI concerns out of this class entirely.
//
// Usage:
//   HistoryNavigator nav(m_history);
//
//   // User presses Up:
//   if (!nav.isNavigating()) nav.saveTypedText(m_input->text());
//   m_input->setText(nav.back());
//
//   // User presses Down:
//   m_input->setText(nav.forward());   // returns savedText() at index 0
//
//   // User types anything:
//   nav.reset();
// -----------------------------------------------------------------------------
class HistoryNavigator {
public:
    explicit HistoryNavigator(const QStringList& history)
        : m_history(history) {}

    // Call before the first Up-arrow to snapshot what the user was typing.
    void saveTypedText(const QString& text) {
        if (m_index == 0)
            m_savedText = text;
    }

    // Move one step toward older entries. Returns the entry text, or the
    // current entry text if already at the oldest entry.
    QString back() {
        if (m_index < m_history.count())
            m_index++;

       
        return currentText();
    }

    // Move one step toward newer entries. At index 0, returns the saved text.
    QString forward() {
        if (m_index > 0)
            m_index--;
        return currentText();
    }

    // True when the user has navigated away from the live input (index > 0).
    bool isNavigating() const { return m_index > 0; }

    // The text shown at the current navigation position.
    // At index 0, returns the saved typed text (live input).
    QString currentText() const {
        if (m_index == 0)
            return m_savedText;
        int pos = m_history.count() - m_index;
        if (pos >= 0 && pos < m_history.count())
            return m_history[pos];
        return m_savedText;
    }

    // Reset navigation - call when the user types, submits, or clears.
    void reset() {
        m_index     = 0;
        m_savedText.clear();
    }

    // The text that was saved before navigation started.
    QString savedText() const { return m_savedText; }

    // Current index (0 = live, 1 = most recent, N = Nth from end).
    int index() const { return m_index; }

private:
    const QStringList& m_history;   // read-only reference - never mutated here
    int     m_index     = 0;
    QString m_savedText;
};
