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
//  Pattern set
//  -----------
//  Facts:      "<X> has <N> <things>"
//              "<X> has <N> more|fewer|less <things> than <Y>"
//              "<X> has <N> times as many <things> as <Y>"
//              "There are <N> <things> [in|on|at <place>]"
//              "Each|Every <container> has <N> <things>"      (a rate)
//  Actions:    "<X> eats|loses|spends|... <N> [<things>]"     (subtract)
//              "<X> buys|finds|gets|... <N> [more] [<things>]"(add)
//              "<X> gives <N> [<things>] to <Y>"              (transfer)
//              "<X> gives <Y> <N> [<things>]"                 (transfer, dative)
//  Questions:  "How many [<things>] does <X> have [now|left|...]?"
//              "How many [<things>] do they have altogether|in total|...?"
//              "How many more <things> does <X> have than <Y>?"
//              "How many <things> are there|are in the <place>?"
//              "What is the total [number of] <things>?"
//
//  Sentences split on '.', '?', '!' and further on "and" when what follows
//  starts a new clause. A subject may be a pronoun or omitted entirely, in
//  which case it resolves to the most recently mentioned owner; the noun may be
//  omitted too when the problem only counts one kind of thing.
//
//  Design stance 1: FAIL LOUDLY. A sentence that matches no pattern aborts the
//  solve and is reported verbatim, rather than being silently dropped and
//  yielding a confidently wrong number. Every refusal is a precise bug report
//  for the next pattern to add. Likewise an ambiguity ("how many does Tom
//  have?" with both apples and oranges on the table) asks which, rather than
//  picking one.
//
//  Design stance 2: VERBS ARE ARITHMETIC, NOTHING MORE. "ate" subtracts; it
//  does not check the object is edible. "Tom has 9 bricks. He ate 3 bricks."
//  answers 6. Guarding that would mean shipping and maintaining a noun ontology
//  to reject sentences nobody types by accident, and the cost lands on every
//  legitimate problem. The verb tables in the .cpp are therefore the whole
//  feature - adding a verb is one word, no new code path.
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
