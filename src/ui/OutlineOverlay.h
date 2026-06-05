#include <QPropertyAnimation>
#include <QRect>
#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QEvent>



class OutlineOverlay : public QWidget {
    Q_OBJECT
        Q_PROPERTY(QRect outlineRect READ outlineRect WRITE setOutlineRect)
public:
    explicit OutlineOverlay(QWidget* parent);
    QRect outlineRect() const;
    void setOutlineRect(const QRect& r);

    void glideTo(const QRect& target);
    // Called by FocusGlow to tell us a drag is starting (e.g., when user presses on overlay)
    void startDrag();

signals:
    void dropRequested(QWidget* target);  // emitted on mouse release with widget under center

    void clearOutline();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;

    void mouseReleaseEvent(QMouseEvent* e) override;



private:
    QRect m_rect;
    QPoint m_dragStartGlobal;
    QRect m_dragStartRect;
    QPropertyAnimation* m_anim = nullptr;
    bool m_dragging;
    QPoint m_dragOffset;
};