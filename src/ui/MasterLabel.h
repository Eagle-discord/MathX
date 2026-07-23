#pragma once
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include "../settings/Settings.h"
#include "../constants/Theme.h"

// -- MasterLabel -----------------------------------------------------------
// Base class for every terminal-output QLabel. Adds an opt-in "widget
// inspector" mode: when behavior/developer/widgetInspector is on, hovering
// glows the label and clicking opens the Widget Editor for it instead of
// performing the label's normal click behavior.
//
// Subclasses get this for free just by inheriting MasterLabel (directly, or
// via CopyableLabel) instead of QLabel. Any Q_PROPERTY a subclass declares
// is automatically picked up by the Widget Editor via Qt's meta-object
// system - no manual registration needed.
class MasterLabel : public QLabel {
    Q_OBJECT
public:
    explicit MasterLabel(QWidget* parent = nullptr) : QLabel(parent) {}
    explicit MasterLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {}

signals:
    // Bubbled up through OutputArea to MainWindow, which opens the editor page.
    void editRequested(QWidget* self);

protected:
    // Subclasses that override mouse events should call this FIRST and
    // return early if it returns true - inspector mode takes over the click.
    bool handleInspectorClick(QMouseEvent* e) {
        if (!inspectorEnabled() || e->button() != Qt::LeftButton)
            return false;
        emit editRequested(this);
        e->accept();
        return true;
    }

    void enterEvent(QEnterEvent* e) override {
        if (inspectorEnabled()) setGlow(true);
        QLabel::enterEvent(e);
    }
    void leaveEvent(QEvent* e) override {
        if (inspectorEnabled()) setGlow(false);
        QLabel::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (handleInspectorClick(e)) return;
        QLabel::mousePressEvent(e);
    }

    bool inspectorEnabled() const {
        return Settings::instance().get("behavior/developer/widgetInspector").toBool();
    }

private:
    void setGlow(bool on) {
        if (on) {
            auto* glow = new QGraphicsDropShadowEffect(this);
            glow->setColor(QColor(Theme::ACCENT()));
            glow->setBlurRadius(14);
            glow->setOffset(0, 0);
            setGraphicsEffect(glow);
            setCursor(Qt::PointingHandCursor);
        }
        else {
            setGraphicsEffect(nullptr);
            unsetCursor();
        }
    }
};