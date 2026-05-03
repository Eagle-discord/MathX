#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QVariantMap>
#include <shapes/GeoCard.h>
#include "RunState.h"   
#include <boost/multiprecision/cpp_int.hpp>

using boost::multiprecision::cpp_int;

struct Progress {
    QStringList text;
    QString tag;
};

class OutputArea : public QScrollArea {
    Q_OBJECT
public:
    explicit OutputArea(QWidget* parent = nullptr);

    void addInputLine(const QString& expr);
    void showProgress(cpp_int percent);   // 0..100
    void hideProgress();

    void addResultLine(const QString& text, const QString& type);
    void addGeoCard(GeoCard* card);
    void addSplash();
    void addSeparator();
    void clearAll();

    void addPromptRequest(const QString& paramName);
    void addPromptAnswer(const QString& paramName, const QString& value);

public slots:
    void addSimplified(const QString& before, const QStringList& after);
    

signals:
    void calcFinish(RunState state, QString result);


private:
    QLabel* m_progressLabel = nullptr;
    QWidget* m_container = nullptr;
    QVBoxLayout* m_layout = nullptr;

    void scrollToBottom();
};