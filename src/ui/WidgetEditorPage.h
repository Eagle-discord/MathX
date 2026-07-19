#pragma once
#include <QWidget>
#include <QPointer>
#include <QLineEdit>

class QVBoxLayout;
class QLabel;

// -- WidgetEditorPage ------------------------------------------------------
// Full-screen inspector for a single MasterLabel-derived widget. Shows the
// widget's class name centered at the top, and one editable row per Qt
// meta-property the widget (or any base class) declares via Q_PROPERTY.
//
// Reached by clicking any MasterLabel while the widget inspector setting
// (behavior/developer/widgetInspector) is enabled. Reusable for any QWidget
// subclass with Q_PROPERTY entries — not limited to labels, though terminal
// text is the only scope wired up for now.
class WidgetEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit WidgetEditorPage(QWidget* parent = nullptr);

    // Points the editor at a new target and rebuilds the property list.
    // Safe to call repeatedly — each call fully replaces the previous view.
    void inspect(QWidget* target);

signals:
    void backClicked();

private:
    void rebuild();
    QWidget* buildRowForProperty(const QMetaProperty& prop, QWidget* target);
    QWidget* buildSectionHeader(const QString& title);
    QWidget* buildStyleSheetTextColorRow(QWidget* target);
    QLineEdit* m_searchEdit = nullptr;
    void applyFilter(const QString& query);
    QPointer<QWidget> m_target;
    QVBoxLayout* m_propsLayout = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
};