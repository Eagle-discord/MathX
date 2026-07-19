#include "TreeParser.h"
#include <QSet>
#include <stdexcept>

namespace {

// Recognised function names (superset that also covers what ExprTree::eval and
// the polynomial extractor understand). Unknown identifiers become variables.
const QSet<QString>& knownFunctions() {
    static const QSet<QString> fns = {
        "sin","cos","tan","sinh","cosh","tanh","asin","acos","atan",
        "asinh","acosh","atanh","atan2","sinr","cosr","tanr","asinr","acosr","atanr",
        "sqrt","cbrt","abs","log","ln","log2","exp","floor","ceil","round","sign",
        "min","max","pow","mod","hypot","fact","gcd","hcf","lcm","ncr","npr","logbase"
    };
    return fns;
}
const QSet<QString>& constants() {
    static const QSet<QString> c = { "pi", "e", "tau", "inf" };
    return c;
}

// A minimal char cursor. The input is assumed already normalised (brackets to
// parens, implicit-mul inserted) by the caller when needed — but the parser
// also tolerates spaces itself.
struct Cursor {
    QString s;
    int pos = 0;

    void ws() { while (pos < s.size() && s[pos].isSpace()) ++pos; }
    QChar peek() { ws(); return pos < s.size() ? s[pos] : QChar(0); }
    QChar peekRaw() const { return pos < s.size() ? s[pos] : QChar(0); }
    bool eof() { ws(); return pos >= s.size(); }
    QChar get() { ws(); return pos < s.size() ? s[pos++] : QChar(0); }
};

struct ParseError { QString msg; };

class P {
public:
    explicit P(const QString& src) { c.s = src; }

    NodePtr parseAll() {
        NodePtr n = addsub();
        c.ws();
        if (c.pos != c.s.size())
            throw ParseError{ "Unexpected trailing input" };
        return n;
    }

private:
    Cursor c;

    NodePtr addsub() {
        NodePtr left = muldiv();
        for (;;) {
            QChar op = c.peek();
            if (op == '+') { c.get(); left = AST::bin(NodeKind::Add, left, muldiv()); }
            else if (op == '-') { c.get(); left = AST::bin(NodeKind::Sub, left, muldiv()); }
            else break;
        }
        return left;
    }

    // Detects implicit multiplication: a primary immediately followed by
    // something that can start another primary (number, name, '(') with no
    // operator between.
    bool startsPrimary(QChar ch) {
        return ch.isLetterOrNumber() || ch == '(' || ch == '.';
    }

    NodePtr muldiv() {
        NodePtr left = unary();
        for (;;) {
            QChar op = c.peek();
            if (op == '*') { c.get(); left = AST::bin(NodeKind::Mul, left, unary()); }
            else if (op == '/') { c.get(); left = AST::bin(NodeKind::Div, left, unary()); }
            else if (startsPrimary(op)) {
                // implicit multiply, e.g. 2x, 3(x+1), (x)(x)
                left = AST::bin(NodeKind::Mul, left, unary());
            }
            else break;
        }
        return left;
    }

    NodePtr unary() {
        QChar op = c.peek();
        if (op == '-') { c.get(); return AST::neg(unary()); }
        if (op == '+') { c.get(); return unary(); }
        return power();
    }

    NodePtr power() {
        NodePtr base = postfix();
        if (c.peek() == '^') {
            c.get();
            NodePtr exp = unary();          // right-associative
            return AST::bin(NodeKind::Pow, base, exp);
        }
        return base;
    }

    NodePtr postfix() {
        NodePtr n = primary();
        while (c.peek() == '!') {
            c.get();
            n = AST::func("fact", { n });   // n! -> fact(n)
        }
        return n;
    }

    NodePtr primary() {
        QChar ch = c.peek();

        if (ch == '(') {
            c.get();
            NodePtr inner = addsub();
            if (c.peek() != ')') throw ParseError{ "Missing ')'" };
            c.get();
            return inner;
        }

        if (ch.isDigit() || ch == '.') return number();

        if (ch.isLetter()) return identifier();

        throw ParseError{ "Unexpected character" };
    }

    NodePtr number() {
        c.ws();
        int start = c.pos;
        bool hasDot = false;
        while (c.pos < c.s.size()) {
            QChar d = c.s[c.pos];
            if (d.isDigit()) { ++c.pos; continue; }
            if (d == '.' && !hasDot) { hasDot = true; ++c.pos; continue; }
            // scientific notation
            if ((d == 'e' || d == 'E') && c.pos + 1 < c.s.size()) {
                QChar nx = c.s[c.pos + 1];
                if (nx == '+' || nx == '-' || nx.isDigit()) {
                    ++c.pos;
                    if (c.s[c.pos] == '+' || c.s[c.pos] == '-') ++c.pos;
                    while (c.pos < c.s.size() && c.s[c.pos].isDigit()) ++c.pos;
                    break;
                }
            }
            break;
        }
        QString tok = c.s.mid(start, c.pos - start);
        try {
            return AST::num(BigDec(tok.toStdString()));
        } catch (...) {
            throw ParseError{ "Invalid number" };
        }
    }

    NodePtr identifier() {
        c.ws();
        int start = c.pos;
        while (c.pos < c.s.size() &&
               (c.s[c.pos].isLetterOrNumber() || c.s[c.pos] == '_'))
            ++c.pos;
        QString id = c.s.mid(start, c.pos - start);
        QString lid = id.toLower();

        // function call?
        if (knownFunctions().contains(lid) && c.peek() == '(') {
            c.get();  // consume '('
            std::vector<NodePtr> args;
            if (c.peek() != ')') {
                args.push_back(addsub());
                while (c.peek() == ',') { c.get(); args.push_back(addsub()); }
            }
            if (c.peek() != ')') throw ParseError{ "Missing ')' in function call" };
            c.get();
            return AST::func(lid, std::move(args));
        }

        if (constants().contains(lid)) return AST::var(lid);

        // Otherwise it's a variable. Keep original case for display, but the
        // extractor lower-cases when it matters.
        return AST::var(id);
    }
};

} // namespace

NodePtr TreeParser::parse(const QString& expr, bool& ok, QString* err) {
    try {
        P p(expr);
        NodePtr root = p.parseAll();
        ok = true;
        return root;
    } catch (const ParseError& e) {
        ok = false;
        if (err) *err = e.msg;
        return nullptr;
    } catch (const std::exception& e) {
        ok = false;
        if (err) *err = QString::fromStdString(e.what());
        return nullptr;
    }
}
