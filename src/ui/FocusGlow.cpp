#include "FocusGlow.h"
#include <QApplication>
#include <QCursor>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include "OutlineOverlay.h"

// ---------- FocusGlow constants ----------
static const int kW = 220;
static const int kH = 110;
static const int kR = 7;
static const QColor kGreen(0, 232, 122);
static const QColor kDim(0, 160, 80, 180);
static const QColor kBg(8, 12, 10, 240);

// ---------- FocusGlow implementation ----------
FocusGlow::FocusGlow(QWidget* parent, QWidget* overlayContainer) : QWidget(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(kW, kH);
    setCursor(Qt::SizeAllCursor);
    setFocusPolicy(Qt::NoFocus);

    if (overlayContainer)
        m_overlay = new OutlineOverlay(overlayContainer);
    else
        m_overlay = new OutlineOverlay(this);

    connect(m_overlay, &OutlineOverlay::dropRequested, this, &FocusGlow::onOverlayDropped);
    connect(this, &QObject::destroyed, m_overlay, &QObject::deleteLater);
}

FocusGlow::~FocusGlow() {
    // m_overlay is deleted automatically by the destroyed signal connection
}
void FocusGlow::onOverlayDropped(QWidget* target) {
    if (target) {
        target->setFocus(Qt::MouseFocusReason);
        // onFocusChanged will automatically update tracked widget
    }
    else {
        onFocusChanged(nullptr, QApplication::focusWidget());
    }
}
void FocusGlow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    m_overlay->show();
    connect(qApp, &QApplication::focusChanged, this, &FocusGlow::onFocusChanged);
    onFocusChanged(nullptr, QApplication::focusWidget());
}

void FocusGlow::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    m_overlay->hide();
    disconnect(qApp, &QApplication::focusChanged, this, &FocusGlow::onFocusChanged);
    if (m_tracked) {
        m_tracked->removeEventFilter(this);
        m_tracked = nullptr;
    }
    m_overlay->clearOutline();
    update();
}

void FocusGlow::onFocusChanged(QWidget* /*old*/, QWidget* now) {
    if (now == this || now == m_overlay) {
        if (m_tracked) {
            m_tracked->removeEventFilter(this);
            m_tracked = nullptr;
            m_overlay->clearOutline();
            update();
        }
        return;
    }

    if (m_tracked) m_tracked->removeEventFilter(this);
    m_tracked = now;
    if (m_tracked) {
        m_tracked->installEventFilter(this);
        updateOverlay();
    }
    else {
        m_overlay->clearOutline();
        update();
    }
}

bool FocusGlow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_tracked && (ev->type() == QEvent::Move || ev->type() == QEvent::Resize)) {
        updateOverlay();
    }
    return false;
}

void FocusGlow::updateOverlay() {
    if (!m_overlay || !m_tracked || m_tracked.isNull()) {
        if (m_tracked.isNull()) m_overlay->clearOutline();
        return;
    }

    QRect globalRect(m_tracked->mapToGlobal(QPoint(0, 0)), m_tracked->size());
    QWidget* overlayParent = m_overlay->parentWidget();
    if (!overlayParent) return;

    // Convert global top‑left to parent coordinates, then set size
    QPoint parentTopLeft = overlayParent->mapFromGlobal(globalRect.topLeft());
    QRect localRect(parentTopLeft, globalRect.size());

    if (localRect == m_lastLocalRect) return;
    m_lastLocalRect = localRect;

    m_overlay->glideTo(localRect);
    update(); // refresh info panel
}
QWidget* FocusGlow::widgetAtPanelCenter() const {
    bool wasTransparent = testAttribute(Qt::WA_TransparentForMouseEvents);
    const_cast<FocusGlow*>(this)->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    QPoint center = geometry().center();
    QWidget* hit = QApplication::widgetAt(center);

    const_cast<FocusGlow*>(this)->setAttribute(Qt::WA_TransparentForMouseEvents, wasTransparent);

    // Walk up to skip container shells and the panel itself
    auto isContainerShell = [](QWidget* w) -> bool {
        if (!w) return false;
        const char* cls = w->metaObject()->className();
        return (strcmp(cls, "QWidget") == 0 && w->objectName().isEmpty())
            || strcmp(cls, "QStackedWidget") == 0
            || strcmp(cls, "QScrollAreaViewport") == 0;
        };
    while (hit && (hit == this || hit == m_overlay || isContainerShell(hit)))
        hit = hit->parentWidget();
    return hit;
}

void FocusGlow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(kGreen, 1));
    p.setBrush(kBg);
    p.drawRoundedRect(QRectF(0.5, 0.5, kW - 1, kH - 1), kR, kR);

    m_closeBtn = QRect(kW - 22, 4, 18, 16);
    p.setFont(QFont("Consolas", 9));
    p.setPen(kDim);
    p.drawText(m_closeBtn, Qt::AlignCenter, "\xc3\x97");

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 232, 122, 110));
    for (int i = -1; i <= 1; ++i)
        p.drawEllipse(QPointF(kW / 2.0 + i * 7, 10), 2.0, 2.0);

    p.setPen(QPen(QColor(0, 100, 50, 100), 1));
    p.drawLine(8, 22, kW - 8, 22);

    const int lm = 10, lh = 19, tw = kW - lm * 2;
    int y = 28;

    if (!m_tracked) {
        p.setFont(QFont("Consolas", 8));
        p.setPen(kDim);
        p.drawText(QRect(0, 30, kW, kH - 30),
            Qt::AlignHCenter | Qt::AlignTop, "following cursor...");
        return;
    }

    QString cls = m_tracked->metaObject()->className();
    QString name = m_tracked->objectName();
    QString row1 = name.isEmpty() ? cls : cls + " · " + name;
    if (row1.length() > 28) row1 = row1.left(25) + "...";

    QString row2 = QString("%1 × %2 px").arg(m_tracked->width()).arg(m_tracked->height());
    QPoint gp = m_tracked->mapToGlobal(QPoint(0, 0));
    QString row3 = QString("pos  %1,  %2").arg(gp.x()).arg(gp.y());
    QString row4 = (m_tracked->isVisible() ? "visible" : "hidden") +
        QString(m_tracked->hasFocus() ? "  · focused" : "");

    p.setFont(QFont("Consolas", 8, QFont::Bold));
    p.setPen(kGreen);
    p.drawText(QRect(lm, y, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, row1); y += lh;
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    p.drawText(QRect(lm, y, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, row2); y += lh;
    p.setFont(QFont("Consolas", 8));
    p.setPen(kDim);
    p.drawText(QRect(lm, y, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, row3); y += lh;
    p.setPen(m_tracked->hasFocus() ? kGreen : kDim);
    p.drawText(QRect(lm, y, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, row4);
}

void FocusGlow::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    if (m_closeBtn.contains(e->pos())) {
        hide();
        emit closedByUser();
        e->accept();
        return;
    }
    m_dragging = true;
    m_dragOffset = e->globalPos() - frameGeometry().topLeft();
    e->accept();
}

void FocusGlow::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) {
        QWidget::mouseMoveEvent(e);
        return;
    }
    move(e->globalPos() - m_dragOffset);
    e->accept();
}

void FocusGlow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        QWidget* target = widgetAtPanelCenter();
        if (target) {
            target->setFocus(Qt::MouseFocusReason);
            // onFocusChanged will be triggered automatically by the focus change
        }
        e->accept();
    }
}
