#pragma once
#include <QWidget>

// Full-window "Features" page: a categorised, scrollable catalogue of what the
// calculator can do. Every entry is a runnable example — clicking it returns to
// the terminal and evaluates it (runExample), so the page doubles as a
// no-manual tour that fits the project's low-friction philosophy. Self-contained
// back button (backClicked), mirroring GeoModeWidget / WidgetEditorPage.
class FeatureViewerPage : public QWidget {
    Q_OBJECT
public:
    explicit FeatureViewerPage(QWidget* parent = nullptr);

signals:
    void backClicked();
    void runExample(const QString& expr);
};
