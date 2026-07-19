#pragma once
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QRandomGenerator>
#include "GhostLineEdit.h"

// -- DemoTyper -----------------------------------------------------------------
// Types example expressions into the input bar's GHOST layer, one character at a
// time, as if someone were demonstrating the app. Purely a showcase: it never
// runs anything, never touches history, and never leaves state behind.
//
// -- Why the ghost, not the text -----------------------------------------------
// The first version drove m_input->setText() per character. It worked, but every
// integration point then needed a guard:
//   - onRun() read the field and would SUBMIT the demo's half-typed text as if
//     the user had written it -- into history, into ans, into the output.
//   - The completer offered suggestions for OUR text, and Tab accepted them:
//     the user finishing a sentence they never started.
//   - stop() had to clear the field, racing the user's first keystroke.
//
// Painting into the ghost makes all three IMPOSSIBLE rather than guarded:
// text() stays empty for the whole demo, so onRun submits nothing, the completer
// has nothing to complete, and there is no text to clear. Not needing a guard
// beats remembering one. It also buys the visual honesty for free -- the ghost
// renders in Theme::MUTED, which is what tells a first-time user "this isn't
// really there".
//
// -- Idle resume ---------------------------------------------------------------
// The reel plays at startup, stops the instant the user does anything, and comes
// back after `idleMs` of no real input -- but ONLY while the field is empty.
//
// That last condition is the whole design. "Idle for 5 seconds" is not the same
// as "not using the app": reading a result, thinking about the next expression,
// or looking at a geo card are all long idles with the field focused. But an
// EMPTY field means the user is not mid-thought -- there's no half-written
// expression to interrupt. Resuming over a half-typed line would be hostile, and
// would also fight the completer, which owns the ghost whenever text() is
// non-empty.
//
// Restart, not resume: a new cycle begins at line 0. Someone returning after a
// pause has forgotten what it showed, and picking up mid-word looks like a
// glitch rather than a demo.
// -----------------------------------------------------------------------------

class DemoTyper : public QObject {
    // No Q_OBJECT: this class declares no signals, and its slots are timer ticks
    // -- which connect fine as lambdas. Adding the macro to a header-only class
    // means listing the header in the target's sources for AUTOMOC to find it:
    // easy to get wrong, no benefit.
public:
    explicit DemoTyper(GhostLineEdit* target, QObject* parent = nullptr)
        : QObject(parent), m_target(target)
    {
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, [this]() { tick(); });

        m_idle.setSingleShot(true);
        connect(&m_idle, &QTimer::timeout, this, [this]() { onIdle(); });
    }

    // The reel. Curated rather than scraped from the sidebar: this is a trailer,
    // so it wants breadth (algebra / trig / geometry / units / bignum) in the
    // first few entries, not depth in one category. Order is fixed, not shuffled
    // -- a first-time viewer should see the range immediately, and randomising
    // means some sessions open with three arithmetic examples in a row.
    void setExpressions(const QStringList& list) { m_lines = list; }

    // How long the input must sit untouched before the reel (re)starts.
    //   ms        -- before the user has touched anything (fresh app)
    //   engagedMs -- after they have (they're probably reading a result)
    void setIdleTimeout(int ms, int engagedMs = 30000) {
        m_idleMs = ms;
        m_engagedIdleMs = engagedMs;
    }

    // First run, after the app has been open long enough to look settled.
    void start(int delayMs = 5000) {
        if (m_lines.isEmpty()) return;
        m_timer.start(delayMs);
        m_running = true;
        m_line = 0;
        m_char = 0;
        m_holding = false;
    }

    // Stop the reel and start counting down to the next one. Call this from
    // every real interaction -- keystrokes, clicks, RUN. Cheap enough to call on
    // every keypress: two timer restarts and a possible ghost clear.
    void interrupted() {
        if (m_running) {
            m_running = false;
            m_timer.stop();
            if (m_target) m_target->setGhost(QString());
        }
        m_engaged = true;
        // Restart the idle countdown even if we weren't running -- every
        // interaction pushes the next demo further out.
        m_idle.start(idleWindow());
    }

    bool isRunning() const { return m_running; }

private:
    // Once the user has actually used the app, wait considerably longer before
    // showing off again. onRun() CLEARS the input, so a plain 5 s window would
    // start typing examples five seconds after every result -- while the user is
    // reading it. An empty field means "not mid-thought", but right after a
    // result it does not mean "not busy".
    int idleWindow() const { return m_engaged ? m_engagedIdleMs : m_idleMs; }

    void onIdle() {
        // Only over an empty field. See the class note: an empty field is the
        // signal that the user isn't mid-thought.
        if (!m_target || !m_target->text().isEmpty()) {
            m_idle.start(idleWindow());   // check again later
            return;
        }
        start(0);
    }

    void tick() {
        if (!m_running || !m_target) return;

        // The user typed in the gap between ticks. Yield: a ghost drawn after
        // their text would look like a completion we never computed, and Tab
        // would accept it.
        if (!m_target->text().isEmpty()) { interrupted(); return; }

        const QString& line = m_lines[m_line];

        // -- typing --
        if (!m_holding && m_char < line.size()) {
            m_target->setGhost(line.left(++m_char));
            // Jittered, not uniform. An even cadence reads as a progress bar,
            // not a person; the variance is what makes it look typed.
            // Punctuation gets a beat longer, the way real typing does.
            const QChar c = line[m_char - 1];
            int delay = 38 + QRandomGenerator::global()->bounded(34);
            if (c == '(' || c == '=' || c == ' ') delay += 45;
            m_timer.start(delay);
            return;
        }

        // -- hold, so it can be read --
        if (!m_holding) {
            m_holding = true;
            m_timer.start(m_holdMs);
            return;
        }

        // -- erase --
        // Backwards rather than blanking: everything else here animates, so a
        // snap to empty is the one abrupt moment and reads as a glitch. Erasing
        // runs ~3x faster than typing because that's what people do -- deleting
        // isn't the interesting part.
        if (m_char > 0) {
            m_target->setGhost(line.left(--m_char));
            m_timer.start(14 + QRandomGenerator::global()->bounded(10));
            return;
        }

        m_holding = false;
        m_line = (m_line + 1) % m_lines.size();
        m_timer.start(280);   // beat of empty field between examples
    }

    GhostLineEdit* m_target = nullptr;
    QStringList m_lines;
    QTimer  m_timer;   // drives the typing animation
    QTimer  m_idle;    // counts down to the next reel after an interruption
    int     m_line = 0;
    int     m_char = 0;
    bool    m_running = false;
    bool    m_holding = false;
    int     m_holdMs = 1500;
    // Two windows on purpose -- see idleWindow(). The first is for a fresh app
    // nobody has touched; the second is for someone who has used it and is
    // probably reading a result.
    int     m_idleMs = 5000;
    int     m_engagedIdleMs = 30000;
    bool    m_engaged = false;   // has the user interacted at all this session?
};