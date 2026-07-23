#pragma once
#include <QObject>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QStringList>
#include "../input/ShapePrompt.h"
#include "../input/InputHandler.h"

// -- PromptController ----------------------------------------------------------
// Owns all shape-prompt state and is the single authority over:
//   - whether a prompt is active
//   - which parameter is currently being collected
//   - the collected values so far
//   - the placeholder text shown in m_input during prompting
//   - the label text shown in m_promptLbl
//
// MainWindow hands it a QLabel* and QLineEdit* at construction so the
// controller can update them directly, keeping placeholder management
// in one place instead of scattered across five methods.
//
// Signals:
//   paramReady(param, value)  - a parameter was accepted; caller echoes it
//   promptComplete(expr)      - all params collected; caller should call run(expr)
//   promptCancelled()         - user pressed Escape; caller resets UI
//   needsMoreInput(param)     - invalid input; caller should re-show the prompt
// -----------------------------------------------------------------------------
class PromptController : public QObject {
    Q_OBJECT

public:
    explicit PromptController(QLabel* promptLbl, QLineEdit* input, QObject* parent = nullptr)
        : QObject(parent), m_promptLbl(promptLbl), m_input(input)
    {}

    // -- Query -----------------------------------------------------------------
    bool isActive()      const { return m_prompt.isActive(); }
    QString currentParam() const { return m_prompt.nextParam(); }

    // Parameters collected so far, as numeric values, so later prompts
    // (e.g. "b = a") can resolve earlier ones by name.
    QMap<QString, double> collectedVars() const {
        QMap<QString, double> vars;
        for (auto it = m_prompt.collected.constBegin(); it != m_prompt.collected.constEnd(); ++it)
            vars[it.key()] = it.value().toDouble();
        return vars;
    }

    // -- Start -----------------------------------------------------------------
    // Begin collecting parameters for the given prompt.
    // Emits nothing - caller should call advanceUI() after to show first param.
    void start(const ShapePrompt& prompt) {
        m_prompt = prompt;
        showCurrentParam();
    }

    // -- Cancel ----------------------------------------------------------------
    void cancel() {
        m_prompt = ShapePrompt{};
        hidePromptUI();
        emit promptCancelled();
    }

    // -- Submit a value for the current parameter ------------------------------
    // Returns false and emits needsMoreInput if value is invalid.
    // Returns true and advances (or completes) on success.
    bool submit(const QString& rawValue, double resolvedValue) {
        if (!isActive()) return false;

        QString param      = m_prompt.nextParam();
        QString displayVal = QString::number(resolvedValue, 'g', 10);

        m_prompt.collected[param] = displayVal;
        m_prompt.currentIndex++;

        emit paramReady(param, displayVal);

        if (m_prompt.isActive()) {
            // More parameters to collect
            showCurrentParam();
        } else {
            // Done - build and hand off the complete expression
            QString fullExpr = InputHandler::buildExpr(m_prompt);
            m_prompt = ShapePrompt{};
            hidePromptUI();
            emit promptComplete(fullExpr);
        }
        return true;
    }

    // -- Reset (e.g. after onClear) --------------------------------------------
    void reset() {
        m_prompt = ShapePrompt{};
        hidePromptUI();
    }

signals:
    void paramReady(const QString& param, const QString& value);
    void promptComplete(const QString& fullExpr);
    void promptCancelled();
    void needsMoreInput(const QString& param, const QString& reason);

private:
    void showCurrentParam() {
        QString param = m_prompt.nextParam();
        m_promptLbl->setText(param + " =");
        m_promptLbl->show();
        m_input->setPlaceholderText(
            QString("enter value for %1...").arg(param));
        m_input->setFocus();
    }

    void hidePromptUI() {
        m_promptLbl->hide();
        m_promptLbl->clear();
        m_input->setPlaceholderText("Enter expression, equation, or shape...");
    }

    ShapePrompt  m_prompt;
    QLabel*      m_promptLbl = nullptr;
    QLineEdit*   m_input     = nullptr;
};
