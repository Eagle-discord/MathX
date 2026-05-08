#include "BigNum.h"
#include <QRegularExpression>
#include <stdexcept>
#include <QThread>
#include <functional>
#include <iostream>
#include <fstream>


// bignum factorial
QString BigNum::bigFactorial(BigInt n, std::function<void(int)> progressCallback) {
    if (n < 0) throw std::runtime_error("Factorial of negative number");    
    BigInt result = 1;
    BigInt total = n;
    for (int i = 2; i <= n; ++i) {

        if (QThread::currentThread()->isInterruptionRequested())
            return QString("Factorial operation cancelled by user");
        result *= i;

        if (progressCallback && i % 100 == 0) {
            int percent = static_cast<int>((i * 100) / total);
            progressCallback(percent);
         
        }
    }
    return QString::fromStdString(result.str());
}

// ── Boost BigDec expression evaluator (no variables) ─────────────────────────
struct BTok {
    QString s; int pos = 0;
    void  skip()  { while (pos < s.size() && s[pos].isSpace()) ++pos; }
    QChar peek()  { skip(); return pos < s.size() ? s[pos] : QChar(0); }
    QChar get()   { skip(); return pos < s.size() ? s[pos++] : QChar(0); }
    bool  atEnd() { skip(); return pos >= s.size(); }
    QString readWord() {
        skip(); QString w;
        while (pos < s.size() && (s[pos].isLetterOrNumber() || s[pos]=='_'))
            w += s[pos++];
        return w;
    }
};

static BigDec bExpr(BTok&);
static BigDec bTerm(BTok&);
static BigDec bPow (BTok&);
static BigDec bPrim(BTok&);

static BigDec bCallFn(const QString& name, BTok& t) {
    if (t.peek() != '(') throw std::runtime_error("Expected '('");
    t.get();
    QVector<BigDec> args;
    if (t.peek() != ')') {
        args.append(bExpr(t));
        while (t.peek() == ',') { t.get(); args.append(bExpr(t)); }
    }
    t.skip();
    if (t.peek() != ')') throw std::runtime_error("Missing ')'");
    t.get();

    if (name=="sqrt")  return BigNum::sqrt(args[0]);
    if (name=="abs")   return BigNum::abs(args[0]);
    if (name=="log")   return BigNum::log10(args[0]);
    if (name=="ln")    return BigNum::log(args[0]);
    if (name=="exp")   return BigNum::exp(args[0]);
    if (name=="floor") return BigNum::floor(args[0]);
    if (name=="ceil")  return BigNum::ceil(args[0]);
    if (name=="sin")   return BigNum::sin(args[0]*BigNum::PI/180);
    if (name=="cos")   return BigNum::cos(args[0]*BigNum::PI/180);
    if (name=="sinr")  return BigNum::sin(args[0]);
    if (name=="cosr")  return BigNum::cos(args[0]);
    if (args.size()>=2) {
        if (name=="pow") return BigNum::pow(args[0],args[1]);
    }
    throw std::runtime_error("Unknown bignum function: " + name.toStdString());
}

static BigDec bPrim(BTok& t) {
    t.skip();
    if (t.peek() == '(') {
        t.get(); BigDec v = bExpr(t); t.skip();
        if (t.peek() == ')') t.get();
        return v;
    }
    if (t.peek().isDigit() || t.peek() == '.') {
        QString n;
        while (t.peek().isDigit()) n += t.get();
        if (t.peek() == '.') { n += t.get(); while (t.peek().isDigit()) n += t.get(); }
        return BigDec(n.toStdString());
    }
    if (t.peek() == '-') { t.get(); return -bPrim(t); }
    if (t.peek().isLetter()) {
        QString word = t.readWord(); t.skip();
        if (word=="pi")  return BigNum::PI;
        if (word=="e")   return BigNum::E;
        if (word=="tau") return 2*BigNum::PI;
        if (t.peek()=='(') return bCallFn(word.toLower(), t);
        throw std::runtime_error("Unknown bignum identifier: " + word.toStdString());
    }
    throw std::runtime_error("Unexpected char in bignum expression");
}

static BigDec bPow(BTok& t) {
    BigDec base = bPrim(t); t.skip();
    if (t.peek() == '^') {
        t.get(); BigDec exp = bPow(t);
        return BigNum::pow(base, exp);
    }
    return base;
}

static BigDec bTerm(BTok& t) {
    BigDec v = bPow(t);
    while (true) {
        t.skip(); QChar op = t.peek();
        if (op != '*' && op != '/') break;
        t.get(); BigDec r = bPow(t);
        if (op == '*') v = BigDec(v*r);
        else { if (r==0) throw std::runtime_error("Division by zero"); v = BigDec(v/r); }
    }
    return v;
}

static BigDec bExpr(BTok& t) {
    BigDec v = bTerm(t);
    while (true) {
        t.skip(); QChar op = t.peek();
        if (op != '+' && op != '-') break;
        t.get(); BigDec r = bTerm(t);
        if (op == '+') v = BigDec(v+r); else v = BigDec(v-r);
    }
    return v;
}

QString BigNum::bigEval(const QString& input, bool& ok) {
    try {
        QString s = input.simplified().replace("**","^");
        // normalise brackets
        s.replace('[','('); s.replace(']',')');
        s.replace('{','('); s.replace('}',')');
        // insert implicit multiply
        QString r;
        for (int i = 0; i < s.size(); ++i) {
            r += s[i];
            if (i+1 < s.size()) {
                QChar c=s[i], n=s[i+1];
                if ((c.isDigit()||c==')')&&(n.isLetter()||n=='(')) r += '*';
            }
        }
        BTok t; t.s = r; t.pos = 0;
        BigDec v = bExpr(t); t.skip();
        if (!t.atEnd()) { ok=false; return {}; }
        ok = true;
        return fmt(v);
    } catch (...) { ok=false; return {}; }
}
