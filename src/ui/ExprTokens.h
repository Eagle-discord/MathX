#pragma once
//  ExprTokens - a reusable model of the *numeric literals* inside an expression
//  string, with the geometry needed to hit-test them against a mouse position.
//
//  This is the data type that makes "drag a number to change it" possible:
//
//      "2 * (3.50 + x)"
//         ^      ^
//         |      +-- NumberToken{ offset=5, length=4, value=3.50, decimals=2 }
//         +--------- NumberToken{ offset=0, length=1, value=2,    decimals=0 }
//
//  Given a click at pixel x, `hitTest` returns the index of the token whose
//  rendered span contains (or is nearest to) that x. Dragging then rewrites
//  only that token's substring, preserving the rest of the expression exactly -
//  including spacing, parentheses, and any non-numeric text.
//
//  Decimal preservation matters: scrubbing "3.50" must yield "3.60", not "3.6".
//  We remember how many decimals the user originally typed and re-format to the
//  same width.

#include <QString>
#include <QVector>
#include <QFontMetrics>

// ---------------------------------------------------------------------------
//  A single numeric literal found in the expression.
// ---------------------------------------------------------------------------
struct NumberToken {
    int     offset = 0;   // index into the source string where the literal starts
    int     length = 0;   // number of characters the literal spans
    double  value = 0.0; // parsed numeric value
    int     decimals = 0;   // digits after '.', so we can re-render at same precision
    bool    negative = false; // literal itself carried a leading '-' we captured

    int end() const { return offset + length; }
};

// ---------------------------------------------------------------------------
//  ExprTokens - tokenise once, then hit-test and rewrite many times.
// ---------------------------------------------------------------------------
class ExprTokens {
public:
    ExprTokens() = default;
    explicit ExprTokens(const QString& expr) { retokenize(expr); }

    // Rebuild the token list from a (possibly edited) expression.
    void retokenize(const QString& expr);

    const QString& expression() const { return m_expr; }
    const QVector<NumberToken>& tokens()     const { return m_tokens; }
    bool                         isEmpty()    const { return m_tokens.isEmpty(); }
    int                          count()      const { return m_tokens.size(); }

    // Map a pixel offset (relative to the start of the rendered text) to the
    // index of the nearest number token, or -1 if there are no tokens.
    //
    // `fm` must be the metrics of the font the text is actually drawn with, and
    // `textStartX` the pixel where character 0 begins (i.e. subtract any left
    // padding / margin before calling).
    //
    // `maxDistancePx` limits how far outside a token we still count as a hit;
    // pass a negative value to always snap to the closest token.
    int hitTest(int pixelX, const QFontMetrics& fm,
        int maxDistancePx = 24) const;

    // Pixel span [startX, endX) that token `index` occupies, for drawing an
    // underline/highlight. Returns false if index is out of range.
    bool tokenPixelSpan(int index, const QFontMetrics& fm,
        int& startX, int& endX) const;

    // Produce a new expression string with token `index` replaced by `newValue`,
    // rendered with that token's original decimal count.
    QString withValueAt(int index, double newValue) const;

    // The step size to use when scrubbing token `index`. Scales with the token's
    // magnitude and precision so dragging "0.05" moves in hundredths while
    // dragging "1200" moves in larger units.
    double stepFor(int index) const;

    // Format a value using token `index`'s original decimal count.
    QString formatLike(int index, double value) const;

private:
    QString              m_expr;
    QVector<NumberToken> m_tokens;
};