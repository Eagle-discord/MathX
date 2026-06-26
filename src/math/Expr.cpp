#include "Expr.h"
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>
#include <cctype>
#include "NaturalLanguage.h"

// -- Helper: normalise brackets (same as before) -------------------------------
static QString normaliseBrackets(QString s) {
    s.replace('[', '('); s.replace(']', ')');
    s.replace('{', '('); s.replace('}', ')');
    s.replace(QChar(0x2329), '('); s.replace(QChar(0x232A), ')');
    s.replace(QChar(0x27E8), '('); s.replace(QChar(0x27E9), ')');
    s.replace(QChar(0x2308), '('); s.replace(QChar(0x2309), ')');
    s.replace(QChar(0x230A), '('); s.replace(QChar(0x230B), ')');
    return s;
}
static int nextNonSpace(const QString& s, int start)
{
    while (start < s.size() && s[start].isSpace())
        ++start;

    return start;
}
static QString insertImplicitMul(const QString& s) {
    QString r;
    int len = s.size();
    for (int i = 0; i < len; ++i) {
        r += s[i];
        // Skip spaces as the “current” character – they never trigger multiplication
        if (s[i].isSpace()) continue;

        // Find the next non‑space character
        int j = i + 1;
        while (j < len && s[j].isSpace()) ++j;
        if (j >= len) continue;

        QChar c = s[i];
        QChar n = s[j];
        bool insert = false;

        // Original rules, but applied to the next non‑space char
        if ((c.isDigit() || c == ')') && (n.isLetter() || n == '('))
            insert = true;
        else if (c.isLetter() && n.isDigit())
            insert = true;
        else if (c == ')' && n.isDigit())
            insert = true;
        else if (c == ')' && n == '(')
            insert = true;
        else if (c == '!' && (n == '(' || n.isDigit() || n.isLetter()))
            insert = true;

        if (insert) {
            r += '*';
        }
    }
    return r;
}

static double callBuiltin(const QString& name, const QVector<double>& args) {
    auto need = [&](int n) {
        if (args.size() < n) throw std::runtime_error(name.toStdString() + " needs " + std::to_string(n) + " argument(s)");
        };
    auto d2r = [](double d) { return d * M_PI / 180.0; };
    auto r2d = [](double r) { return r * 180.0 / M_PI; };

    if (name == "sin") {
        need(1);
        return std::sin(d2r(args[0])); 
    }
    if (name == "cos") { need(1); return std::cos(d2r(args[0])); }
    if (name == "tan") { need(1); return std::tan(d2r(args[0])); }
    if (name == "sinh") { need(1); return std::sinh(args[0]); }
    if (name == "cosh") { need(1); return std::cosh(args[0]); }
    if (name == "tanh") { need(1); return std::tanh(args[0]); }
    if (name == "asin") { need(1); return r2d(std::asin(args[0])); }
    if (name == "acos") { need(1); return r2d(std::acos(args[0])); }
    if (name == "atan") { need(1); return r2d(std::atan(args[0])); }
    if (name == "asinh") { need(1); return r2d(std::asinh(args[0])); }
    if (name == "acosh") { need(1); return r2d(std::acosh(args[0])); }
    if (name == "atanh") { need(1); return r2d(std::atanh(args[0])); }
    if (name == "atan2") { need(2); return r2d(std::atan2(args[0], args[1])); }
    if (name == "sinr") { need(1); return std::sin(args[0]); }
    if (name == "cosr") { need(1); return std::cos(args[0]); }
    if (name == "tanr") { need(1); return std::tan(args[0]); }
    if (name == "asinr") { need(1); return std::asin(args[0]); }
    if (name == "acosr") { need(1); return std::acos(args[0]); }
    if (name == "atanr") { need(1); return std::atan(args[0]); }
    if (name == "sqrt") { need(1); if (args[0] < 0) throw std::runtime_error("sqrt of negative"); return std::sqrt(args[0]); }
    if (name == "cbrt") { need(1); return std::cbrt(args[0]); }
    if (name == "abs") { need(1); return std::abs(args[0]); }
    if (name == "log") { need(1); if (args[0] <= 0) throw std::runtime_error("log non-positive"); return std::log10(args[0]); }
    if (name == "ln") { need(1); if (args[0] <= 0) throw std::runtime_error("ln non-positive"); return std::log(args[0]); }
    if (name == "log2") { need(1); return std::log2(args[0]); }
    if (name == "logbase") { need(2); return std::log(args[1]) / std::log(args[0]); }
    if (name == "exp") { need(1); return std::exp(args[0]); }
    if (name == "floor") { need(1); return std::floor(args[0]); }
    if (name == "ceil") { need(1); return std::ceil(args[0]); }
    if (name == "round") { need(1); return std::round(args[0]); }
    if (name == "sign") { need(1); return (args[0] > 0) - (args[0] < 0); }
    if (name == "min") {
        need(2);
        double result = args[0];
        for (int i = 1; i <= args.size(); i++) {
            result = std::min(result, args[i]);
        }
        return result; 
    }
    if (name == "max") {
        need(2);
        double result = args[0];
        for (int i = 1; i <= args.size() - 1; i++) {
            result = std::max(result, args[i]);
        }
        return result;
    }
    if (name == "pow") { need(2); return std::pow(args[0], args[1]); }
    if (name == "mod") { need(2); return std::fmod(args[0], args[1]); }
    if (name == "hypot") { need(2); return std::hypot(args[0], args[1]); }
    if (name == "fact") {
        need(1);
        throw std::runtime_error("__BIGFACT__:" + std::to_string((long long)args[0]));
    }
    if (name == "lcm") {
        need(2);

        auto gcd_ll = [](long long a, long long b) {
            while (b) {
                a %= b;
                std::swap(a, b);
            }
            return a;
            };

        long long result = std::llabs((long long)args[0]);

        for (int i = 1; i < args.size(); ++i) {
            long long b = std::llabs((long long)args[i]);

            if (result == 0 || b == 0)
                return 0;

            result = result / gcd_ll(result, b) * b;
        }

        return (double)result;
    }
    if (name == "avg") {
        need(1);

        double total = 0;

        for (double x : args)
            total += x;

        return total / args.size();
    }
    if (name == "prod") {
        need(1);

        double result = 1;

        for (double x : args)
            result *= x;

        return result;
    }
    if (name == "sum") {
        need(1);

        double total = 0;

        for (double x : args)
            total += x;

        return total;
    }
    if (name == "gcd" || name == "hcf") {
        need(2);

        long long result = std::llabs((long long)args[0]);

        for (int i = 1; i < args.size(); ++i) {
            long long b = std::llabs((long long)args[i]);

            while (b) {
                result %= b;
                std::swap(result, b);
            }
        }

        return (double)result;
    }
    if (name == "ncr" || name == "c" || name == "nCr") {
        need(2);
        int n = (int)args[0], r = (int)args[1];
        if (r < 0 || r > n) return 0;
        r = std::min(r, n - r);
        long long res = 1;
        for (int i = 0; i < r; ++i) res = res * (n - i) / (i + 1);
        return (double)res;
    }
    if (name == "npr") {
        need(2);
        int n = (int)args[0], r = (int)args[1];
        if (r < 0 || r > n) return 0;
        long long res = 1;
        for (int i = 0; i < r; ++i) res *= (n - i);
        return (double)res;
    }
    throw std::runtime_error("Unknown function '" + name.toStdString() + "'");
}

// -- Token types and tokenizer ------------------------------------------------
enum class TokenType {
    Number, Variable, Add, Sub, Mul, Div, Pow, Mod,
    LParen, RParen, Factorial, Percent, Comma,
    Function, End, Invalid
};

struct Token {
    TokenType type;
    QString text;
    double value;
};

class Tokenizer {
public:
    Tokenizer(const QString& s, const VarMap& vars) : m_str(s), m_pos(0), m_vars(vars) {
        next();
    }

    Token current() const { return m_curr; }
    void next() { m_curr = readToken(); }

private:
    QString m_str;
    int m_pos;
    VarMap m_vars;
    Token m_curr;

    void skipWhitespace() {
        while (m_pos < m_str.size() && m_str[m_pos].isSpace())
            m_pos++;
    }

    Token readToken() {
        skipWhitespace();
        if (m_pos >= m_str.size())
            return { TokenType::End };

        QChar ch = m_str[m_pos];
        if (ch.isDigit() || ch == '.')
            return readNumber();
        if (ch.isLetter())
            return readIdentifier();

        m_pos++;
        switch (ch.toLatin1()) {
        case '+': return { TokenType::Add };
        case '-': return { TokenType::Sub };
        case '*': return { TokenType::Mul };
        case '/': return { TokenType::Div };
        case '^': return { TokenType::Pow };
        case '%': return { TokenType::Percent };
        case '(': return { TokenType::LParen };
        case ')': return { TokenType::RParen };
        case ',': return { TokenType::Comma };
        case '!': return { TokenType::Factorial };
        default:  return { TokenType::Invalid };
        }
    }

    Token readNumber() {
        int start = m_pos;
        bool hasDot = false;
        while (m_pos < m_str.size()) {
            QChar c = m_str[m_pos];
            if (c.isDigit()) { m_pos++; continue; }
            if (c == '.' && !hasDot) { hasDot = true; m_pos++; continue; }
            if ((c == 'e' || c == 'E') && m_pos + 1 < m_str.size()) {
                QChar next = m_str[m_pos + 1];
                if (next == '+' || next == '-' || next.isDigit()) {
                    m_pos++; // consume e/E
                    if (m_str[m_pos] == '+' || m_str[m_pos] == '-') m_pos++;
                    while (m_pos < m_str.size() && m_str[m_pos].isDigit()) m_pos++;
                    break;
                }
            }
            break;
        }
        QString numStr = m_str.mid(start, m_pos - start);
        bool ok;
        double val = numStr.toDouble(&ok);
        return { TokenType::Number, numStr, ok ? val : 0.0 };
    }

    Token readIdentifier() {
        int start = m_pos;
        while (m_pos < m_str.size() && (m_str[m_pos].isLetterOrNumber() || m_str[m_pos] == '_'))
            m_pos++;
        QString id = m_str.mid(start, m_pos - start).toLower();

        // List of known functions (must match callBuiltin)
        static QSet<QString> functions = {
            "sin","cos","tan","sinh","cosh","tanh","asin","acos","atan",
            "asinh","acosh","atanh","atan2","sinr","cosr","tanr","asinr","acosr","atanr",
            "sqrt","cbrt","abs","log","ln","log2","exp","floor","ceil","round","sign",
            "min","max","pow","mod","hypot","fact","gcd","hcf","lcm","ncr","npr","c","logbase","sum", "prod", "avg",
        };
        // Known constants – treat as variables so the parser can handle them
        static const QSet<QString> constants = { "pi", "e", "tau", "inf" };
        if (constants.contains(id))
            return { TokenType::Variable, id };
        if (functions.contains(id))
            return { TokenType::Function, id };
        else if (m_vars.contains(id))
            return { TokenType::Variable, id };
        else
            return { TokenType::Invalid };  // Unknown identifier -> error
    }
};

// -- Recursive descent parser -------------------------------------------------
class Parser {
public:
    Parser(Tokenizer& tokenizer, const VarMap& vars) : m_tok(tokenizer), m_vars(vars) {}

    double parseExpression() {
        return parseAddSub();
    }

private:
    Tokenizer& m_tok;
    const VarMap& m_vars;

    void consume(TokenType expected) {
        if (m_tok.current().type != expected)
            throw std::runtime_error("Unexpected token");
        m_tok.next();
    }

    double parseAddSub() {
        double left = parseMulDiv();
        while (true) {
            TokenType tt = m_tok.current().type;
            if (tt == TokenType::Add) {
                m_tok.next();
                left += parseMulDiv();
            }
            else if (tt == TokenType::Sub) {
                m_tok.next();
                left -= parseMulDiv();
            }
            else break;
        }
        return left;
    }

    double parseMulDiv() {
        double left = parsePow();
        while (true) {
            TokenType tt = m_tok.current().type;
            if (tt == TokenType::Mul) {
                m_tok.next();
                left *= parsePow();
            }
            else if (tt == TokenType::Div) {
                m_tok.next();
                double right = parsePow();
                if (right == 0.0) throw std::runtime_error("Division by zero");
                left /= right;
            }
            else if (tt == TokenType::Mod) {
                m_tok.next();
                double right = parsePow();
                if (right == 0.0) throw std::runtime_error("Modulo by zero");
                left = std::fmod(left, right);
            }
            else break;
        }
        return left;
    }

    double parsePow() {
        double left = parseUnary();
        if (m_tok.current().type == TokenType::Pow) {
            m_tok.next();
            double right = parsePow(); // right‑associative
            left = std::pow(left, right);
        }
        return left;
    }

    double parseUnary() {
        Token t = m_tok.current();
        if (t.type == TokenType::Add) {
            m_tok.next();
            return parseUnary();
        }
        else if (t.type == TokenType::Sub) {
            m_tok.next();
            return -parseUnary();
        }
        else {
            return parsePrimary();
        }
    }

    double applyPostfix(double val) {
        while (true) {
            if (m_tok.current().type == TokenType::Factorial) {
                m_tok.next();
                if (val < 0 || std::fmod(val, 1.0) != 0.0)
                    throw std::runtime_error("Factorial of non‑integer");
                long long n = static_cast<long long>(val);
                if (n > 20)
                    throw std::runtime_error("Factorial too large inside expression (max 20)");
                long long res = 1;
                for (long long i = 2; i <= n; ++i) res *= i;
                val = static_cast<double>(res);
            }
            else if (m_tok.current().type == TokenType::Percent) {
                m_tok.next();
                val /= 100.0;
            }
            else {
                break;
            }
        }
        return val;
    }

    double parsePrimary() {
        Token t = m_tok.current();
        if (t.type == TokenType::Number) {
            m_tok.next();
            return applyPostfix(t.value);
        }
        else if (t.type == TokenType::Variable) {

            QString name = t.text;
            m_tok.next();
            // Constants
            if (name == "pi") return M_PI;
            if (name == "e") return M_E;
            if (name == "tau") return 2.0 * M_PI;
            if (name == "inf") return std::numeric_limits<double>::infinity();

            double val = m_vars.value(name, 0.0);
            return applyPostfix(val);
        }
        else if (t.type == TokenType::LParen) {
            m_tok.next();
            double val = parseExpression();
            consume(TokenType::RParen);
            return applyPostfix(val);
        }
        else if (t.type == TokenType::Function) {
            QString name = t.text;
            m_tok.next();
            consume(TokenType::LParen);
            QVector<double> args;
            if (m_tok.current().type != TokenType::RParen) {
                args.append(parseExpression());
                while (m_tok.current().type == TokenType::Comma) {
                    m_tok.next();
                    args.append(parseExpression());
                }
            }
            consume(TokenType::RParen);
            return callBuiltin(name, args);
        }
        else {
            throw std::runtime_error("Unexpected token in primary");
        }
    }
};

static QString replaceXWithTimes(const QString& s) {
    QString result;
    int len = s.length();
    for (int i = 0; i < len; ++i) {
        QChar c = s[i];
        if (c == 'x' || c == 'X') {
            bool leftIsDigit = (i > 0 && s[i - 1].isDigit());
            bool rightIsDigit = (i + 1 < len && s[i + 1].isDigit());
            bool leftIsSpace = (i > 0 && s[i - 1].isSpace());
            bool rightIsSpace = (i + 1 < len && s[i + 1].isSpace());
            if ((leftIsDigit && rightIsDigit) || (leftIsSpace && rightIsSpace)) {
                result += '*';
                continue;
            }
        }
        result += c;
    }
    return result;
}

// -- Public interface ---------------------------------------------------------
double Expr::eval(const QString& input, bool& ok) {
    QString expr = input;
    expr = NaturalLanguage::preprocess(expr);

    return evalWith(expr, {}, ok);
}

double Expr::evalWith(const QString& input, const VarMap& vars, bool& ok) {
    try {
        QString s = input.simplified();
        s = normaliseBrackets(s);
        s = replaceXWithTimes(s);
        s = s.replace("**", "^");
        s = insertImplicitMul(s);
        
        Tokenizer tokenizer(s, vars);
        Parser parser(tokenizer, vars);
        double result = parser.parseExpression();
        if (tokenizer.current().type != TokenType::End)
            throw std::runtime_error("Extra tokens after expression");
        ok = true;
        return result;
    }
    catch (const std::exception& e) {
        ok = false;
        return 0.0;
    }
}

QSet<QString> Expr::detectVariables(const QString& expr) {
    QSet<QString> vars;
    static const QSet<QString> exclude = {
        "pi","e","tau","inf","sin","cos","tan","asin","acos","atan","atan2",
        "sinr","cosr","tanr","asinr","acosr","atanr","sqrt","cbrt","abs",
        "log","ln","log2","exp","floor","ceil","round","sign","min","max",
        "pow","mod","hypot","fact","ncr","npr","gcd","lcm","logbase", "sum", "prod", "avg"
    };
    QRegularExpression re(R"([a-df-z])"); // a-d, f-z (exclude 'e')
    auto it = re.globalMatch(expr.toLower());
    while (it.hasNext()) {
        auto m = it.next();
        QString v = m.captured(0);
        if (!exclude.contains(v)) vars.insert(v);
    }
    return vars;
}
static QString Expr::replaceSuperscripts(const QString& s)
{
    QString out;

    auto isSup = [](QChar c)
        {
            return QString("⁰¹²³⁴⁵⁶⁷⁸⁹").contains(c);
        };

    auto toDigit = [](QChar c)
        {
            switch (c.unicode())
            {
            case L'⁰': return '0';
            case L'¹': return '1';
            case L'²': return '2';
            case L'³': return '3';
            case L'⁴': return '4';
            case L'⁵': return '5';
            case L'⁶': return '6';
            case L'⁷': return '7';
            case L'⁸': return '8';
            case L'⁹': return '9';
            default: return '?';
            }
        };

    for (int i = 0; i < s.size(); ++i)
    {
        if (isSup(s[i]))
        {
            out += '^';

            while (i < s.size() && isSup(s[i]))
            {
                out += toDigit(s[i]);
                ++i;
            }

            --i;
        }
        else
        {
            out += s[i];
        }
    }

    return out;
}

QString Expr::preprocess(const QString& expr) {
    QString s = expr.simplified().replace("**", "^");
    s = replaceSuperscripts(s);
    s = normaliseBrackets(s);
    return insertImplicitMul(s);
}