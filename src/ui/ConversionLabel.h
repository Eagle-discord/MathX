#include <QLabel>
#include <QTimer>
#include <QApplication>
#include <QClipBoard>
#include <QMouseEvent>


// Conversion result label: single‑click copies value, double‑click shows/hides formula
class ConversionLabel : public QLabel {
    Q_OBJECT
public:
    ConversionLabel(const QString& resultNum, const QString& formulaStr,
        const QFont& font, const QString& color,
        const QString& extraStyle = "")
        : QLabel(resultNum), m_result(resultNum), m_formula(formulaStr),
        m_showFormula(false), m_copied(false)
    {
        setFont(font);
        setStyleSheet("background:transparent; color:" + color + ";" + extraStyle + ";padding-left:22px");
        setWordWrap(true);
        setCursor(Qt::PointingHandCursor);
        setText(resultNum);
        m_resetTimer = new QTimer(this);
        m_resetTimer->setSingleShot(true);
        connect(m_resetTimer, &QTimer::timeout, this, &ConversionLabel::resetCopy);
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::RightButton) {
            // Single click: copy the numeric result (not the formula)
            QApplication::clipboard()->setText(m_result);
            m_copied = true;
            setText("Copied!");
            m_resetTimer->start(800);
        }
        else if (e->button() == Qt::LeftButton && !m_formula.isEmpty()) {
            toggleFormula();
        }
        QLabel::mousePressEvent(e);
    }

private:
    void toggleFormula() {
        m_showFormula = !m_showFormula;
        if (m_showFormula) {
            setText(m_result + "  [ " + m_formula + " ]");
        }
        else {
            setText(m_result);
        }
    }

    void resetCopy() {
        m_copied = false;
        setText(m_showFormula ? (m_result + "  [ " + m_formula + " ]") : m_result);
    }

    QString m_result;
    QString m_formula;
    bool m_showFormula;
    bool m_copied;
    QTimer* m_resetTimer;
};