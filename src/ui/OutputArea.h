#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QVariantMap>
#include <shapes/GeoCard.h>
#include "RunState.h"   
#include <boost/multiprecision/cpp_int.hpp>
#include "..\theme\Theme.h"
#include <QProgressBar>


static const QString C_BG = Theme::BG;
static const QString C_SURFACE = Theme::SURFACE;
static const QString C_CARD = Theme::CARD;
static const QString C_BORDER = Theme::BORDER;
static const QString C_ACCENT = Theme::ACCENT;
static const QString C_ACCENT_DIM = Theme::ACCENT_DIM;
static const QString C_TEXT = Theme::TEXT;
static const QString C_MUTED = Theme::MUTED;
static const QString C_ERR = Theme::ERROR;
static const QString C_VRED = "#dc1e14";
static const QString C_DRED = "#ad102f";
static const QString C_OUT = "#0d100e";   // slightly darker than surface; not in Theme
static const QString C_DIM = Theme::DIM;
static const QString C_INFO = Theme::INFO;
static const QString C_WARN = Theme::WARN;
static const QString C_PURPLE = Theme::PURPLE;
static const QString C_ALG = Theme::ALG;
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

    void addResultLine(const QString& text, const QString& type,
        const QString& copyText = QString(), const QString& formula = QString());
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


private:
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_progressLabel = nullptr;
    QWidget* m_container = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QLabel*       m_relNotes      = nullptr;
    void scrollToBottom();
};