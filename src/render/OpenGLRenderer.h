#pragma once

#include "Renderer.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QMap>
#include <memory>
#include "../shapes/RenderShape.h"
#include "../shapes/ShapeRegistry.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFramebufferObject>
#include <QVector3D>
#include <QElapsedTimer>
#include <QColor>
#include <QImage>
#include <vector>
#include "MathText.h"



class OpenGLRenderer : public QOpenGLWidget, public Renderer, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit OpenGLRenderer(QWidget* parent = nullptr);
    ~OpenGLRenderer() override;

    void setShapeColor(float r, float g, float b);
    void setBackgroundColor(float r, float g, float b);
    // Renderer interface
    bool initialize(QWidget* parent) override;
    void setShape(const QString& type, const QMap<QString, double>& params) override;
    void setDarkTheme(bool dark) override;
    void setRotationEnabled(bool enabled) override;
    QWidget* widget() override { return this; }
    void setHorizontalOffset(int offsetPixels);
    void setMouseCtrl(bool enabled) override;
    void clearMesh();

    // Formula walkthrough: pans the camera to a 3/4 view, then flies dimension
    // indicators (l/w/h) to their edges while the matching edge lights up in a
    // matching colour. Triggered by clicking a formula on the properties card.
    // Currently implemented for the cuboid; other shapes no-op.
    void startFormulaAnimation(const QString& shapeType, const QString& formulaKey);
    void stopFormulaAnimation();
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateRotation();
    void drawSphere(float radius, int slices, int stacks);
    void drawCube(float size);
    void drawCylinder(float radius, float height, int segments);
    void drawGlass();
    void drawLegacy();

    // -- Glow / bloom post-process -------------------------------------
    // Compiles the post shaders and builds the fullscreen quad. Sets
    // m_glowReady; if it fails we fall back to direct rendering.
    void initGlow();
    // (Re)allocates the scene + ping-pong bloom framebuffers for a given size.
    void allocateGlowTargets(int w, int h);
    // Renders the current shape (glass or legacy) with GL state as-is.
    void drawScene();
    // Bright-pass -> separable blur -> additive composite to the default FBO.
    void renderGlow();
    void drawFullscreenQuad(QOpenGLShaderProgram& prog);

    // -- Formula walkthrough animation ----------------------------------
    // Colored GL edge highlights, drawn at the end of drawScene().
    void drawAnimHighlights();
    // QPainter labels/leader lines + colored formula readout, drawn on top of
    // the finished GL frame at the end of paintGL().
    void drawAnimOverlay();
    // World -> widget (logical px) using the same view/proj as the scene.
    QPointF projectToScreen(const QVector3D& p) const;
    // 0..1 flash driving the duplication bloom burst: peaks the moment the
    // half-shell starts duplicating, decays fast (gone before faces land).
    float dupFlashIntensity() const;
    // 0..1 progress of a phase starting at `start` lasting `dur` seconds.
    float animPhaseT(float start, float dur) const;
    // (Re)computes indicator edges/texts from m_params. Called on start and
    // whenever params change mid-walkthrough (slider drags), so the labels
    // and highlighted edges track the live shape.
    void rebuildIndicatorGeometry();
    bool m_mousePressed = false;
    bool m_mouseCtrlEnabled = false;
    QPoint m_lastMousePos;
    float m_camDistance = 5.0f; // optional zoom
    std::unique_ptr<RenderShape> m_currentShape;
    QString m_shapeType;
    QMap<QString, double> m_params;
    float m_xRot = 0.0f;
    float m_yRot = 0.0f;
    float m_colorR = 1.0f, m_colorG = 1.0f, m_colorB = 1.0f; // pure white (glow tints it)
    float m_bgR = 0.0f, m_bgG = 0.0f, m_bgB = 0.0f;
    bool m_rotating = true;
    bool m_dark = true;
    QTimer m_rotationTimer;
    // Measures real frame-to-frame time so motion is FRAME-RATE INDEPENDENT:
    // a slow or jittery tick advances the animation further instead of
    // stalling it (fixed per-tick steps read as chop under load).
    QElapsedTimer m_frameClock;
    int m_horizontalOffset = 0;
    RenderMode m_renderMode = RenderMode::Wireframe; // default until a shape is set
    QOpenGLShaderProgram m_glassShader;
    bool m_shaderReady = false;

    // -- Glow / bloom post-process state -------------------------------
    QOpenGLShaderProgram m_prefilterShader;
    QOpenGLShaderProgram m_blurShader;
    QOpenGLShaderProgram m_compositeShader;
    QOpenGLVertexArrayObject m_quadVao;
    QOpenGLBuffer m_quadVbo{ QOpenGLBuffer::VertexBuffer };
    std::unique_ptr<QOpenGLFramebufferObject> m_sceneFbo;        // full-res, MSAA + depth
    std::unique_ptr<QOpenGLFramebufferObject> m_sceneResolveFbo; // MSAA resolve target (sampled)
    std::unique_ptr<QOpenGLFramebufferObject> m_bloomFbo[2];     // half-res ping-pong
    bool  m_glowReady = false;
    float m_glowIntensity = 0.0f;   // shape bloom strength; 0 = off (blur passes skipped)
    float m_glowThreshold = 0.12f;  // bright-pass cutoff
    int   m_bloomPasses = 5;        // H+V blur pairs (wider halo = more)
    float m_exposure = 1.1f;        // glow gain before tone-map rolloff

    // Floor / ground glow (composite pass)
    QVector3D m_floorColor{ 0.12f, 0.125f, 0.145f }; // near-neutral, faintly cool
    float     m_floorIntensity = 1.5f;   // strip is small; needs a little more punch

    // -- Formula walkthrough animation state ----------------------------
    // NOTE: deliberately does NOT touch the camera or auto-rotation. The
    // shape keeps spinning and the annotations re-project every frame, so
    // the walkthrough reads as live 3D rather than a frozen still.
    struct DimIndicator {
        QChar     letter;     // 'l' / 'w' / 'h'
        QString   valueText;  // "= 2.2" (small annotation under the letter)
        QColor    color;
        QVector3D a, b;       // edge endpoints in world space
        float     tStart, tDur;  // phase window within the timeline (seconds)
        // Cached MathText layouts (laying out per frame allocates paths).
        QVector<MathText::Placed> letterGlyphs;
        QVector<MathText::Placed> valueGlyphs;
    };
    std::vector<DimIndicator> m_indicators;

    // The explanation phase: one entry per bracket TERM. Each step builds ONE
    // face (border trace -> fill wipe -> caption) and PERSISTS, so after the
    // three terms half the shell is tinted. The finale then highlights the
    // "2" and the whole half-shell duplicates as ONE rigid block that swings
    // around the box (yaw + flip) onto the opposite three faces, completing
    // the surface area.
    struct FaceTerm {
        QString   term;       // "lw"
        QString   caption;    // "lw = 6.6" (term with current values)
        QColor    color;
        int       group;      // readout colour group (1-based; 0 = plain text)
        QVector3D quad[4];    // the ONE face shown during the term step
        QVector3D center;
        float     tStart, tDur;   // build window (holds at 1 after; no fade)
        float     dupStart;       // when the duplicate block starts moving
        // Cached MathText layouts.
        QVector<MathText::Placed> captionGlyphs;
        qreal captionW = 0;
        QVector<MathText::Placed> headlineGlyphs;   // "Term: lw"
        qreal headlineW = 0;
    };
    std::vector<FaceTerm> m_faceTerms;

    // Readout segments with colour groups, so a face-pair step can light up
    // exactly its own term ("lw") while the rest dims.
    struct ReadoutSeg { QString text; int group; };
    std::vector<ReadoutSeg> m_readout;
    QVector<MathText::Placed> m_readoutGlyphs;      // cached layout
    qreal m_readoutW = 0;
    QVector<MathText::Placed> m_headline2Glyphs;    // "Term: 2"
    qreal m_headline2W = 0;

    bool    m_animActive = false;
    QElapsedTimer m_animClock;

    // Overlay text is rendered into this image with Qt's RASTER engine (crisp
    // grayscale glyph AA), then blitted over the GL frame. The GL paint
    // engine's path antialiasing is far worse for text.
    QImage m_overlayImage;
};