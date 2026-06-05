#include <QPropertyAnimation>
#include <QRect>
#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>

#include "OutlineOverlay.h"

OutlineOverlay::OutlineOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);

    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::SizeAllCursor);
    if (parent) setGeometry(parent->rect());
    raise();
    m_anim = new QPropertyAnimation(this, "outlineRect", this);
    m_anim->setDuration(100);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void OutlineOverlay::resizeEvent(QResizeEvent*) {
    if (parentWidget())
        setGeometry(parentWidget()->rect());
}

QRect OutlineOverlay::outlineRect() const
{
    return m_rect;
}

void OutlineOverlay::setOutlineRect(const QRect& r)
{
    m_rect = r; update();
}

void OutlineOverlay::glideTo(const QRect& target)
{
    if (m_dragging) return; // don't animate while user drags
    QRect currentTarget = (m_anim->state() == QAbstractAnimation::Running)
        ? m_anim->endValue().toRect()
        : m_rect;
    if (qAbs(currentTarget.x() - target.x()) < 2 &&
        qAbs(currentTarget.y() - target.y()) < 2 &&
        qAbs(currentTarget.width() - target.width()) < 2 &&
        qAbs(currentTarget.height() - target.height()) < 2)
        return;

    if (!m_rect.isValid()) {
        setOutlineRect(target);
        return;
    }
    if (m_anim->state() == QAbstractAnimation::Running &&
        m_anim->endValue().toRect() == target)
        return;

    m_anim->stop();
    m_anim->setStartValue(m_rect);
    m_anim->setEndValue(target);
    m_anim->start();

}

void OutlineOverlay::startDrag()
{
    m_dragging = true;
    m_anim->stop();   // stop any ongoing animation

}

void OutlineOverlay::paintEvent(QPaintEvent*) 
{
    if (!m_rect.isValid()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 5; i >= 1; --i) {
        p.setPen(QPen(QColor(0, 232, 122, 14 * i), i * 2 + 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(m_rect.adjusted(-i, -i, i, i));
    }
    p.setPen(QPen(QColor(0, 232, 122), 2));
    p.drawRect(m_rect.adjusted(1, 1, -1, -1));

    const int tick = 8;
    p.setPen(QPen(QColor(0, 255, 140), 2));
    auto corner = [&](int x, int y, int dx, int dy) {
        p.drawLine(x, y, x + dx * tick, y);
        p.drawLine(x, y, x, y + dy * tick);
        };
    corner(m_rect.left(), m_rect.top(), 1, 1);
    corner(m_rect.right(), m_rect.top(), -1, 1);
    corner(m_rect.left(), m_rect.bottom(), 1, -1);
    corner(m_rect.right(), m_rect.bottom(), -1, -1);
}
void OutlineOverlay::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        // Temporarily disable transparency so we can receive move events
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        m_dragStartGlobal = e->globalPos();
        m_dragStartRect = m_rect;
        m_anim->stop();
        e->accept();
    }
}

void OutlineOverlay::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    QPoint delta = e->globalPos() - m_dragStartGlobal;
    QRect newRect = m_dragStartRect.translated(delta);
    setOutlineRect(newRect);
    e->accept();
}

void OutlineOverlay::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        // Restore transparency so clicks go through to underlying widgets
        setAttribute(Qt::WA_TransparentForMouseEvents, true);

        QPoint center = mapToGlobal(m_rect.center());
        QWidget* target = QApplication::widgetAt(center);
        // Skip container shells (same logic)
        auto isContainerShell = [](QWidget* w) -> bool {
            if (!w) return false;
            const char* cls = w->metaObject()->className();
            return (strcmp(cls, "QWidget") == 0 && w->objectName().isEmpty())
                || strcmp(cls, "QStackedWidget") == 0
                || strcmp(cls, "QScrollAreaViewport") == 0;
            };
        while (target && (target == this || isContainerShell(target)))
            target = target->parentWidget();
        emit dropRequested(target);
        e->accept();
    }
}