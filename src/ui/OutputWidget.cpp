#include "OutputWidget.h"
#include <constants/Theme.h>

OutputWidget::OutputWidget(const QString& displayText, const QString& copyText,
    const QString& originalExpr, const QString& formula,
    ResultType type, const QString& color,
    const QString& extraStyle, QWidget* parent)
    : CopyableLabel(displayText, parent),
    m_displayText(displayText), m_originalExpr(originalExpr),
    m_type(type), m_extraStyle(extraStyle)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setWordWrap(false); // Makes a big performance difference
    setFont(Theme::monoFont(9));

    // Only "conv" and "alg" results ever carried a toggleable formula in the
    // original implementation, and only when a formula string was supplied.
    const bool hasDetail = (type == ResultType::conv || type == ResultType::alg) && !formula.isEmpty();

    setupCopyable(copyText, color, formula,
        hasDetail ? DetailTrigger::DoubleClickToggle : DetailTrigger::None,
        HoverHint::WhenCopyable);
}