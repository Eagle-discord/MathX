#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QVariantMap>
#include <shapes/GeoCard.h>
#include "..\constants\RunState.h"   
#include <boost/multiprecision/cpp_int.hpp>
#include "..\constants\Theme.h"
#include <QProgressBar>



using boost::multiprecision::cpp_int;

struct Progress {
    QStringList text;
    QString tag;
};

class OutputArea : public QScrollArea {
    Q_OBJECT
public:
    explicit OutputArea(QWidget* parent = nullptr);
    bool progressState();
    void addInputLine(const QString& expr);
    void showProgress(int percent, const QString& label);   // 0..100
    void hideProgress();
    void addResultLine(const QString& text, const QString& type,
        const QString& copyText = QString(),
        const QString& formula = QString(),
        const QString& originalExpr = QString());
    void addGeoCard(GeoCard* card);
    void addSplash();
    void addSeparator();
    void clearAll();

    void addPromptRequest(const QString& paramName);
    void addPromptAnswer(const QString& paramName, const QString& value);

public slots:
    void addSimplified(const QString& before, const QStringList& after);
    void captureWidget(QWidget* widget);

signals:
    void calcFinish(RunState state, QString result);
    void shapeProjectionRequested(const QString& type, const QMap<QString, double>& params);

private:
    QProgressBar* m_progressBar = nullptr;
    
    QLabel* m_progressLabel = nullptr;
    QWidget* m_container = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QLabel*       m_relNotes      = nullptr;
    void scrollToBottom();
};