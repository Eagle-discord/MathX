#pragma once
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QStyleOptionFrame>
#include <QFontMetrics>
#include <QApplication>
#include <QClipboard>
#include <functional>
#include "../constants/Theme.h"

// -- GhostLineEdit -------------------------------------------------------------
// QLineEdit that paints a greyed-out completion after the cursor, Fish-shell
// style. The suggestion itself is computed elsewhere (Completer); this class
// only knows how to draw a string it's handed.
//
// -- Why a paintEvent override --------------------------------------------------
// QLineEdit gives no access to its internal QTextLine, so there's no supported
// way to append styled text. The alternatives are worse:
//   - A second QLineEdit behind this one: two widgets to keep in sync on font,
//     scroll offset, and margins; drifts the moment either changes.
//   - setPlaceholderText: only shows when the field is EMPTY. Useless here --
//     the whole point is a suggestion after text you've already typed.
//   - QCompleter: gives a popup list, which is a different feature. See the
//     note in Completer.h about why one inline answer beats a dropdown.
// So: draw the ghost ourselves, positioned with the same QFontMetrics the widget
// uses for its own text.
//
// -- The horizontal-scroll caveat ----------------------------------------------
// When the typed text is longer than the field, QLineEdit scrolls its contents
// and there is no public API to ask by how much. Rather than guess and paint the
// ghost in the wrong place, we suppress it entirely once the text approaches the
// right edge -- see paintEvent. A ghost that drifts is worse than none.
// -----------------------------------------------------------------------------

class GhostLineEdit : public QLineEdit {
    // Deliberately no Q_OBJECT: this class declares no signals or slots of its
    // own, and adding the macro to a header-only class means it must be listed
    // in the target's sources for AUTOMOC to find it -- an easy thing to get
    // wrong for zero benefit. Inherited signals (textChanged etc.) work fine
    // without it.
public:
    explicit GhostLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}

    // The suffix to paint after the typed text. Empty hides it.
    void setGhost(const QString& g) {
        if (m_ghost == g) return;
        m_ghost = g;
        update();
    }
    QString ghost() const { return m_ghost; }
    bool hasGhost() const { return !m_ghost.isEmpty(); }

    // Called when a paste containing line breaks arrives. QLineEdit would
    // silently flatten it -- "f(x) = x*5\nf(2)" becomes "f(x) = x*5f(2)" and
    // fails with a confusing parse error. The lines have to go somewhere that
    // can hold them; the handler decides where (MainWindow routes them to the
    // batch panel).
    //
    // A std::function rather than a signal, because a signal would need
    // Q_OBJECT, and Q_OBJECT on a header-only class means remembering to list
    // the header in the target's sources for AUTOMOC. Return true to consume the
    // paste; return false (or set no handler) and it falls through to QLineEdit's
    // normal flattening, so this can't break a field that doesn't care.
    void setMultilinePasteHandler(std::function<bool(const QString&)> h) {
        m_onMultilinePaste = std::move(h);
    }

protected:
    // QLineEdit has no insertFromMimeData() to override -- that's QTextEdit --
    // and paste() isn't virtual. Intercepting the key event is the only hook
    // that runs before the text is mangled.
    //
    // This covers Ctrl+V and Shift+Insert. It does NOT cover the context menu's
    // Paste, which calls QLineEdit::paste() directly with no hook. Pre-empting
    // contextMenuEvent was tried and reverted: it would have to fire whenever the
    // clipboard happened to hold multiple lines, stealing right-click from Copy,
    // Cut, and Select All for reasons the user cannot see. A rare mangled paste
    // beats a right-click that randomly does nothing.
    void keyPressEvent(QKeyEvent* e) override {
        if (e->matches(QKeySequence::Paste) && tryRouteMultiline()) {
            e->accept();
            return;
        }
        QLineEdit::keyPressEvent(e);
    }
    void paintEvent(QPaintEvent* e) override {
        const bool ghostOnEmpty = !m_ghost.isEmpty() && text().isEmpty();

        // Skip the base paint only when a ghost is showing on an empty field.
        // QLineEdit would draw the placeholder in the exact spot the ghost goes
        // and the two would overlap into mush. Skipping is safe HERE because
        // this field is styled `background:transparent; border:none` -- there is
        // no frame or fill to lose, only the placeholder and a cursor that
        // isn't drawn on an unfocused empty field anyway.
        //
        // The obvious alternative -- toggling placeholderText() -- is worse: it
        // calls update() (recursing if done from paintEvent), and
        // PromptController holds a plain QLineEdit* and sets the placeholder
        // through it, so any non-virtual shadow here would be silently bypassed.
        if (!ghostOnEmpty) QLineEdit::paintEvent(e);

        if (m_ghost.isEmpty()) return;

        // Focus is required for a COMPLETION ghost -- it answers "what will Tab
        // do", and Tab needs focus to mean anything. But the demo reel paints
        // through the same channel on an empty field and must show regardless:
        // its whole job is to be seen before the user has touched anything.
        if (!hasFocus() && !text().isEmpty()) return;

        // Only paint when the cursor is at the very end. A ghost sitting in the
        // middle of a line -- after the cursor but before more typed text -- is
        // nonsense, and Tab would do something the user can't predict.
        if (cursorPosition() != text().size()) return;

        const QFontMetrics fm(font());
        const QRect cr = contentsRect();

        // Left edge of the ghost = left edge of the text area + width of what's
        // typed. Mirrors QLineEdit's own layout: contentsRect, inset by the
        // style's frame width, plus its 2px internal padding.
        QStyleOptionFrame opt;
        initStyleOption(&opt);
        const QRect textArea =
            style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);

        int x = textArea.left() + 2 + fm.horizontalAdvance(text());
        const int rightLimit = textArea.right() - 2;

        // Suppress rather than clip. Once we're near the edge QLineEdit may have
        // scrolled its contents by an amount we cannot query, which would put the
        // ghost somewhere arbitrary.
        if (x >= rightLimit) return;

        QPainter p(this);
        p.setFont(font());
        p.setPen(QColor(Theme::MUTED));

        // Elide against the real right edge so a long history suggestion doesn't
        // paint over the frame.
        const QString shown = fm.elidedText(m_ghost, Qt::ElideRight, rightLimit - x);
        p.drawText(QRect(x, cr.top(), rightLimit - x, cr.height()),
            Qt::AlignVCenter | Qt::AlignLeft, shown);
    }

private:
    // True if the clipboard holds something worth routing: at least two
    // non-empty lines. The two-line floor matters -- text editors love to add a
    // trailing newline, so "2+2\n" is a single expression someone copied, not a
    // batch, and shunting it into the panel for no reason would be maddening.
    bool tryRouteMultiline() {
        if (!m_onMultilinePaste) return false;

        const QClipboard* cb = QApplication::clipboard();
        if (!cb) return false;
        const QString text = cb->text();
        if (!text.contains('\n') && !text.contains('\r')) return false;

        int nonEmpty = 0;
        const auto lines = QStringView(text).split(QLatin1Char('\n'));
        for (const auto& l : lines)
            if (!l.trimmed().isEmpty()) ++nonEmpty;
        if (nonEmpty < 2) return false;

        // Normalise CRLF -- text copied from Windows editors and web pages
        // arrives with \r\n, and the batch editor would render a stray \r as a
        // visible box on every line.
        QString norm = text;
        norm.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));

        return m_onMultilinePaste(norm);
    }

    QString m_ghost;
    std::function<bool(const QString&)> m_onMultilinePaste;
};