#pragma once
#include <QString>

// ---------------------------------------------------------------------------
//  NumberFormat - digit grouping for RESULT DISPLAY ONLY.
//
//  Never apply this to a string that will be read back as a number. Three
//  places do exactly that, and all three break on a grouped number:
//
//    * `ans` is set by re-parsing the result string (MathEngine::evaluate)
//    * the clipboard text is pasted back into the calculator
//    * batch assertions compare computed values
//
//  So grouping happens at the display boundary and everything else keeps
//  seeing the raw value. OutputArea::addResultLine captures its copy text from
//  the unformatted string before calling any of this.
// ---------------------------------------------------------------------------
namespace NumberFormat {

// Group long runs of digits in `text`. Anything that isn't a whole number -
// error messages, proofs, word-problem working, identifiers - passes through
// untouched.
QString groupNumbers(const QString& text);

// A human-scale reading of a large number: "3.3 trillion" for 3298534883330.
// Empty when the value is too small to need one (below a million, where the
// digits speak for themselves) or too large for any common name - a factorial
// with five million digits has no word, and inventing one is worse than
// saying nothing.
QString scaleWord(const QString& text);

} // namespace NumberFormat
