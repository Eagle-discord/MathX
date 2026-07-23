#pragma once
#include <QString>
#include <QVector>

// ---------------------------------------------------------------------------
//  WordProblem - a rule-based solver for elementary arithmetic word problems.
//
//  General word-problem solving is an open NLP problem; this is deliberately
//  NOT that. It is a pattern table over a well-defined subset, built on the
//  observation that every fact in such a problem is (owner, attribute) = value:
//
//      "Timmy has 5 apples"  ->  owner=timmy, attribute=apples, value=5
//
//  which is exactly a multi-word variable name ("apples with timmy"), so the
//  bindings live in the ordinary variable registry and the rest of the engine
//  can use them afterwards.
//
//  v1 pattern set
//  --------------
//  Facts:      "<X> has <N> <things>"
//              "<X> has <N> more|fewer|less <things> than <Y>"
//              "<X> has <N> times as many <things> as <Y>"
//  Questions:  "How many <things> does <X> have?"
//              "How many <things> do they have altogether|in total|in all?"
//              "How many more <things> does <X> have than <Y>?"
//
//  Design stance: FAIL LOUDLY. A sentence that matches no pattern aborts the
//  solve and is reported verbatim, rather than being silently dropped and
//  yielding a confidently wrong number. Every refusal is a precise bug report
//  for the next pattern to add.
// ---------------------------------------------------------------------------
namespace WordProblem {

// One resolved fact, kept so the answer can show its working.
struct Step {
    QString sentence;   // the source sentence
    QString binding;    // "apples with sarah = 5 + 3"
    QString value;      // "8"
};

struct Solution {
    bool            solved = false;
    QString         answer;     // final value, formatted
    QString         question;   // the question sentence, echoed
    QString         detail;     // how the answer was arrived at, e.g. "5 + 8"
    QVector<Step>   working;    // facts in the order they were stated
    QString         failure;    // why we gave up (empty when solved)
};

// Cheap sniff used to gate the solver so ordinary expressions never reach it.
bool looksLikeWordProblem(const QString& text);

Solution solve(const QString& text);

// Render a Solution as the multi-line string shown to the user.
QString format(const Solution& s);

} // namespace WordProblem
