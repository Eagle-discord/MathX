#pragma once
#include <QWidget>
#include <QPointer>
#include <QRect>

class OutlineOverlay; // forward declaration

class FocusGlow : public QWidget {
    Q_OBJECT
public:
    explicit FocusGlow(QWidget* parent = nullptr, QWidget* overlayContainer = nullptr);
    ~FocusGlow();

signals:
    void closedByUser();

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private slots:
    void onFocusChanged(QWidget* old, QWidget* now);
    void onOverlayDropped(QWidget* target);


private:
    bool eventFilter(QObject* obj, QEvent* ev) override;

    void updateOverlay();          // updates outline and info text
    void updateOverlayGeometry();  // ensures overlay covers the right screen
    QWidget* widgetAtPanelCenter() const;

    OutlineOverlay* m_overlay = nullptr;
    QPointer<QWidget> m_tracked;   // the widget currently being highlighted
    QRect m_lastLocalRect;
    bool m_dragging = false;
    QPoint m_dragOffset;
    QRect m_closeBtn;
    QRect m_lastGlobalRect;        // last known outline rect (to avoid redundant updates)
    bool m_overlayDragging = false;
};