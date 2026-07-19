#include "OpenGLRenderer.h"
#include <cmath>
#include "../shapes/RenderShape.h"
#include "../shapes/cube/cube.h"
#include "../shapes/sphere/sphere.h"
#include <memory>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <shapes/cylinder/cylinder.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <shapes/ShapeRegistry.h>
#include <shapes/ShapeDefs.h>
#include <QOpenGLFramebufferObjectFormat>
#include <QVector2D>
#include <QPainter>
#include <QPainterPath>
#include "../constants/Theme.h"
#include "MathText.h"

// Helper to avoid GLU dependency – custom perspective
static void setPerspective(float fovY, float aspect, float zNear, float zFar) {
    float fH = tan(fovY / 360.0f * M_PI) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
}

// -- Explanation timeline -----------------------------------------------
// Term steps build ONE face each and PERSIST (no fade). After the last term,
// the "x2" finale duplicates the built half-shell as one block onto the
// opposite side of the cuboid, completing the surface.
static constexpr float kTermStart = 1.5f;   // first term begins
static constexpr float kTermGap = 1.8f;   // start-to-start spacing
static constexpr float kTermDur = 1.4f;   // build duration per term
static constexpr float kDupStart = kTermStart + 3 * kTermGap + 0.2f;  // 7.1
static constexpr float kDupDur = 1.8f;   // block rotation duration

// -- Ending sequence: hold the completed surface, then a pure white glow
// engulfs the screen and fades back out to reveal the plain cuboid. The
// walkthrough state is cleared BEHIND the white — the reveal shows the shape
// exactly as it was before the animation.
static constexpr float kEndHold = 5.0f;   // hold after the block lands
static constexpr float kWhiteIn = 0.9f;   // white swells over the screen
static constexpr float kWhiteHold = 0.25f;  // fully white beat
static constexpr float kWhiteOut = 1.2f;   // fade back to the scene
static constexpr float kWhiteStart = kDupStart + kDupDur + kEndHold;   // 13.9
static constexpr float kWhiteFull = kWhiteStart + kWhiteIn;
static constexpr float kAnimEnd = kWhiteFull + kWhiteHold + kWhiteOut;

// The duplicated half-shell moves as ONE rigid block: a 180-degree yaw about
// the box centre combined with a vertical flip. At t=1 this composes to the
// exact point reflection (-x,-y,-z), landing the block on the opposite three
// faces — which no single proper rotation can do for a general cuboid.
static QVector3D dupTransform(const QVector3D& v, float t) {
    const float a = float(M_PI) * t;
    const float c = std::cos(a);
    const float s = std::sin(a);
    return { c * v.x() + s * v.z(),
             v.y() * (1.0f - 2.0f * t),
             -s * v.x() + c * v.z() };
}

// Build sub-phases (fractions of a term's window):
static constexpr float kBorderFrac = 0.35f; // border trace
static constexpr float kFillFrom = 0.20f; // fill wipe start
static constexpr float kFillSpan = 0.55f; // fill wipe span
static constexpr float kCapFrom = 0.45f; // caption write start
static constexpr float kCapSpan = 0.40f; // caption write span

OpenGLRenderer::OpenGLRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
    connect(&m_rotationTimer, &QTimer::timeout, this, &OpenGLRenderer::updateRotation);
    // PreciseTimer: the default coarse timer is allowed ~5% slack per tick on
    // Windows, which reads as animation jitter.
    m_rotationTimer.setTimerType(Qt::PreciseTimer);
    m_rotationTimer.start(16);
    m_frameClock.start();

    // Game-loop pacing: when a frame finishes presenting, immediately request
    // the next one (while something is animating). This renders at the
    // monitor's full refresh rate with vsync pacing, instead of being capped
    // and beat-interfered by the 16ms timer. The timer remains as the loop
    // kicker when animation state turns on.
    connect(this, &QOpenGLWidget::frameSwapped, this, [this]() {
        if (m_rotating || m_animActive)
            update();
        });
    // Request 4x multisampling for smoother edges, and UNCAP the frame rate:
    // swap interval 0 disables vsync blocking, so the frameSwapped loop spins
    // as fast as the GPU can render (motion is dt-based, so speed is
    // unaffected — only smoothness improves).
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    fmt.setSwapInterval(0);

    setFormat(fmt);

}

OpenGLRenderer::~OpenGLRenderer() {
    // GL resources must be released with a current context; QOpenGLWidget does
    // not do this automatically for member-owned objects.
    makeCurrent();
    m_sceneFbo.reset();
    m_sceneResolveFbo.reset();
    m_bloomFbo[0].reset();
    m_bloomFbo[1].reset();
    if (m_quadVbo.isCreated()) m_quadVbo.destroy();
    if (m_quadVao.isCreated()) m_quadVao.destroy();
    doneCurrent();
}

bool OpenGLRenderer::initialize(QWidget* /*parent*/) {
    // Already a QWidget, nothing extra needed.
    // This method is called by RenderWidget after creation.
    m_params.clear();
    m_shapeType.clear();
    return true;
}

void OpenGLRenderer::clearMesh() {
    makeCurrent();

    m_currentShape.reset();
    m_shapeType.clear();
    m_params.clear();

    doneCurrent();

    update();

}
void OpenGLRenderer::setShape(const QString& type, const QMap<QString, double>& params) {
    // A different shape invalidates the walkthrough's edge table.
    if (m_animActive && type != m_shapeType)
        stopFormulaAnimation();

    m_params = params;

    if (m_currentShape && m_shapeType == type) {
        // Same shape type (e.g. a slider drag firing many valueChanged
        // events per second) — just update parameters. Recreating the
        // RenderShape here previously destroyed the old one's GPU mesh
        // (VAO/VBO/EBO) without a current GL context, leaking the buffers,
        // and forced a full mesh rebuild + re-upload on every tick.
        m_currentShape->setParameters(params);
        if (m_animActive)
            rebuildIndicatorGeometry();   // keep labels/edges on the live shape
        update();
        return;
    }

    m_shapeType = type;
    makeCurrent();
    m_currentShape = ShapeRegistry::create(type);
    doneCurrent();
    if (m_currentShape) {
        m_currentShape->setParameters(params);
        const ShapeDef* d = ShapeRegistry::def(type);
        m_renderMode = d ? d->renderMode : RenderMode::Wireframe;
    }
    m_xRot = 0.0f;
    m_yRot = 0.0f;

    update();
}

void OpenGLRenderer::setDarkTheme(bool dark) {
    m_dark = dark;
    // Re‑initialize clear color (will be applied in paintGL or initializeGL)
    if (isValid()) {
        makeCurrent();
        glClearColor(m_dark ? 0.1f : 0.9f, m_dark ? 0.1f : 0.9f, m_dark ? 0.1f : 0.9f, 1.0f);
        doneCurrent();
        update();
    }
}
void OpenGLRenderer::setMouseCtrl(bool enabled) {
    m_mouseCtrlEnabled = enabled;
}

void OpenGLRenderer::mousePressEvent(QMouseEvent* event) {
    if ((event->button() == Qt::RightButton || event->button() == Qt::LeftButton) && !m_rotating) {
        m_mousePressed = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLRenderer::mouseMoveEvent(QMouseEvent* event) {
    if (m_mousePressed && m_mouseCtrlEnabled) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        // Sensitivity: 0.5 degrees per pixel
        if (m_shapeType == "circle") {
            m_yRot += delta.x() * 0.5f;
            m_xRot -= delta.y() * 0.5f;
        }
        else {
            m_yRot -= delta.x() * 0.5f;
            m_xRot += delta.y() * 0.5f;
        }
        update();
        event->accept();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLRenderer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::LeftButton) {
        m_mousePressed = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

// Optional: zoom with mouse wheel
void OpenGLRenderer::wheelEvent(QWheelEvent* event) {
    float delta = -event->angleDelta().y() / 60.0f;
    m_camDistance += delta * 0.25f;


    update();
    event->accept();
}

void OpenGLRenderer::setRotationEnabled(bool enabled) {
    m_rotating = enabled;
}
void OpenGLRenderer::setShapeColor(float r, float g, float b) {
    m_colorR = r; m_colorG = g; m_colorB = b;
    update(); // redraw
}

void OpenGLRenderer::setBackgroundColor(float r, float g, float b) {
    m_bgR = r; m_bgG = g; m_bgB = b;
    if (isValid()) {
        makeCurrent();
        glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
        doneCurrent();
        update();
    }
}
void OpenGLRenderer::initializeGL() {
    initializeOpenGLFunctions();

    qDebug() << "initializeGL";

    GLint texUnits;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texUnits);
    qDebug() << "Max texture:" << texUnits;

    glClearColor(m_bgR, m_bgG, m_bgB, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    m_shaderReady =
        m_glassShader.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/glass.vert") &&
        m_glassShader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/glass.frag") &&
        m_glassShader.link();

    if (!m_shaderReady)
        qWarning() << "Glass shader failed:" << m_glassShader.log();

    initGlow();
}

void OpenGLRenderer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);

    if (m_glowReady)
        allocateGlowTargets(w, h);
}
/*  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(m_xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yRot, 0.0f, 1.0f, 0.0f);
    glColor3f(1.0f, 1.0f, 1.0f); // white
    float offsetX = 2.0f * m_horizontalOffset / width();


    if (m_shapeType == "cube") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
        drawCube(1.5f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // restore
    }
    else if (m_shapeType == "sphere") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawSphere(1.0f, 36, 36);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else if (m_shapeType == "cylinder") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawCylinder(0.8f, 1.5f, 36);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}*/

void OpenGLRenderer::paintGL() {
    // Advance time-based state HERE, by real elapsed time (clamped so a stall
    // doesn't jump-cut). Frame-rate independent: rendering faster gives
    // smoother motion at the same speed.
    const float dt = std::min(m_frameClock.restart() / 1000.0f, 0.05f);
    if (m_rotating) {
        m_xRot += 62.0f * dt;   // degrees per second
        m_yRot += 44.0f * dt;
    }

    // Walkthrough finale finished (white fade-out complete): dismantle the
    // animation. The reset already happened visually behind the white.
    if (m_animActive && animPhaseT(kAnimEnd, 0.001f) >= 1.0f)
        stopFormulaAnimation();

    // Fallback path: if the glow pipeline isn't available, render directly to
    // the widget exactly as before.
    if (!m_glowReady || !m_sceneFbo) {
        glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_currentShape) drawScene();
        drawAnimOverlay();
        return;
    }

    // 1) Render the shape into the multisampled scene buffer.
    m_sceneFbo->bind();
    glViewport(0, 0, m_sceneFbo->width(), m_sceneFbo->height());
    glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_currentShape) drawScene();
    m_sceneFbo->release();

    // 1b) Resolve MSAA into the sampleable texture.
    QOpenGLFramebufferObject::blitFramebuffer(
        m_sceneResolveFbo.get(), m_sceneFbo.get(),
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 2) Composite (+ optional bloom, floor glow) to the screen.
    renderGlow();

    // 3) 2D annotation layer (formula walkthrough) on the finished frame.
    drawAnimOverlay();
}

void OpenGLRenderer::drawScene() {
    if (m_renderMode == RenderMode::Glass && m_shaderReady)
        drawGlass();
    else
        drawLegacy();

    drawAnimHighlights();
}

void OpenGLRenderer::initGlow() {
    auto load = [](QOpenGLShaderProgram& p, const char* vs, const char* fs) -> bool {
        return p.addShaderFromSourceFile(QOpenGLShader::Vertex, vs)
            && p.addShaderFromSourceFile(QOpenGLShader::Fragment, fs)
            && p.link();
        };

    const bool ok =
        load(m_prefilterShader, ":/shaders/fullscreen.vert", ":/shaders/prefilter.frag") &&
        load(m_blurShader, ":/shaders/fullscreen.vert", ":/shaders/blur.frag") &&
        load(m_compositeShader, ":/shaders/fullscreen.vert", ":/shaders/composite.frag");

    if (!ok) {
        qWarning() << "Glow shaders failed to compile — using direct render."
            << m_prefilterShader.log() << m_blurShader.log() << m_compositeShader.log();
        m_glowReady = false;
        return;
    }

    // Fullscreen quad (triangle strip) in NDC.
    static const float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    m_quadVao.create();
    m_quadVao.bind();
    m_quadVbo.create();
    m_quadVbo.bind();
    m_quadVbo.allocate(quad, sizeof(quad));
    m_quadVbo.release();
    m_quadVao.release();

    m_glowReady = true;

    // Targets are sized in resizeGL; if the widget was already sized before the
    // context came up, build them now.
    if (width() > 0 && height() > 0)
        allocateGlowTargets(width(), height());
}

void OpenGLRenderer::allocateGlowTargets(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (m_shapeType == "Circle") return;

    // Scene renders into an 8x multisampled FBO (smooth wireframe + highlight
    // edges), which cannot be texture-sampled directly — it resolves into
    // m_sceneResolveFbo, and THAT texture feeds the prefilter/composite.
    QOpenGLFramebufferObjectFormat sceneFmt;
    sceneFmt.setAttachment(QOpenGLFramebufferObject::Depth);
    sceneFmt.setSamples(8);
    m_sceneFbo = std::make_unique<QOpenGLFramebufferObject>(w, h, sceneFmt);

    QOpenGLFramebufferObjectFormat resolveFmt;   // plain colour, sampled
    m_sceneResolveFbo = std::make_unique<QOpenGLFramebufferObject>(w, h, resolveFmt);

    const int bw = std::max(1, w / 2);
    const int bh = std::max(1, h / 2);
    QOpenGLFramebufferObjectFormat bloomFmt;   // colour only, no depth
    m_bloomFbo[0] = std::make_unique<QOpenGLFramebufferObject>(bw, bh, bloomFmt);
    m_bloomFbo[1] = std::make_unique<QOpenGLFramebufferObject>(bw, bh, bloomFmt);

    // Linear filtering + edge clamp so blur taps interpolate smoothly.
    // (The MSAA scene FBO has no sampleable texture — params go on the
    // resolve target instead.)
    auto setTexParams = [this](GLuint tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
    setTexParams(m_sceneResolveFbo->texture());
    setTexParams(m_bloomFbo[0]->texture());
    setTexParams(m_bloomFbo[1]->texture());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::drawFullscreenQuad(QOpenGLShaderProgram& prog) {
    m_quadVao.bind();
    m_quadVbo.bind();
    const int loc = prog.attributeLocation("position");
    if (loc >= 0) {
        prog.enableAttributeArray(loc);
        prog.setAttributeBuffer(loc, GL_FLOAT, 0, 2, 2 * sizeof(float));
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_quadVbo.release();
    m_quadVao.release();
}

void OpenGLRenderer::renderGlow() {
    const int bw = m_bloomFbo[0]->width();
    const int bh = m_bloomFbo[0]->height();

    glDisable(GL_DEPTH_TEST);

    int src = 0;

    // Effective bloom = the (currently zero) base intensity plus the
    // duplication flash: a fast-fading burst while the half-shell copies
    // slide across. The flash uses a HIGH bright-pass threshold so the glow
    // clings to the white-hot duplicate faces, not the whole scene.
    const float flash = dupFlashIntensity();
    const float glowNow = m_glowIntensity + 0.75f * flash;
    const float thresholdNow = (flash > 0.0f) ? 0.6f : m_glowThreshold;

    // Shape bloom is optional (currently off). When disabled, skip the
    // bright-pass and blur entirely; the composite still runs for the floor
    // glow, multiplying whatever stale bloom texture by intensity 0.
    if (glowNow > 0.0f) {
        // -- Bright-pass: scene -> bloomFbo[0] -------------------------
        m_bloomFbo[0]->bind();
        glViewport(0, 0, bw, bh);
        glClear(GL_COLOR_BUFFER_BIT);
        m_prefilterShader.bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneResolveFbo->texture());
        m_prefilterShader.setUniformValue("scene", 0);
        m_prefilterShader.setUniformValue("threshold", thresholdNow);
        drawFullscreenQuad(m_prefilterShader);
        m_prefilterShader.release();
        m_bloomFbo[0]->release();

        // -- Separable Gaussian blur, ping-pong ------------------------
        m_blurShader.bind();
        m_blurShader.setUniformValue("src", 0);
        int dst = 1;
        bool horizontal = true;
        const int steps = m_bloomPasses * 2;
        for (int i = 0; i < steps; ++i) {
            m_bloomFbo[dst]->bind();
            glViewport(0, 0, bw, bh);
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_bloomFbo[src]->texture());
            const QVector2D dir = horizontal ? QVector2D(1.0f / bw, 0.0f)
                : QVector2D(0.0f, 1.0f / bh);
            m_blurShader.setUniformValue("dir", dir);
            drawFullscreenQuad(m_blurShader);
            m_bloomFbo[dst]->release();
            std::swap(src, dst);
            horizontal = !horizontal;
        }
        m_blurShader.release();
        // Blurred bloom now lives in m_bloomFbo[src].
    }

    // -- Composite scene + bloom to the widget's default framebuffer ----
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, width(), height());
    glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_compositeShader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneResolveFbo->texture());
    m_compositeShader.setUniformValue("scene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomFbo[src]->texture());
    m_compositeShader.setUniformValue("bloom", 1);
    m_compositeShader.setUniformValue("intensity", glowNow);
    m_compositeShader.setUniformValue("exposure", m_exposure);
    m_compositeShader.setUniformValue("floorColor", m_floorColor);
    m_compositeShader.setUniformValue("floorIntensity", m_floorIntensity);
    drawFullscreenQuad(m_compositeShader);
    m_compositeShader.release();

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}

// Smoothstep ease for the camera tween and label flights.
static float easeSmooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void OpenGLRenderer::updateRotation() {
    // Loop kicker only: state advances in paintGL by measured frame time.
    // Once a frame is in flight, frameSwapped keeps the loop running at the
    // monitor's refresh rate; this timer just (re)starts it.
    if (m_rotating || m_animActive)
        update();
}

float OpenGLRenderer::animPhaseT(float start, float dur) const {
    const float t = m_animClock.elapsed() / 1000.0f;
    return std::clamp((t - start) / dur, 0.0f, 1.0f);
}

QPointF OpenGLRenderer::projectToScreen(const QVector3D& p) const {
    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -m_camDistance);
    view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);
    QMatrix4x4 proj;
    proj.perspective(45.0f, (float)width() / (float)height(), 0.1f, 100.0f);

    const QVector3D ndc = (proj * view).map(p);   // map() does the perspective divide
    return QPointF((ndc.x() * 0.5 + 0.5) * width(),
        (1.0 - (ndc.y() * 0.5 + 0.5)) * height());
}

void OpenGLRenderer::rebuildIndicatorGeometry() {
    const float w = (float)m_params.value("w", 1.0);
    const float h = (float)m_params.value("h", 1.0);
    const float l = (float)m_params.value("l", 1.0);
    const float hw = w * 0.5f, hh = h * 0.5f, hd = l * 0.5f;

    auto val = [](float v) {
        return QString("= %1").arg(QString::number(v, 'g', 4));
        };

    // Three edges meeting at the front-bottom-right corner — the classic
    // "box dimensions" diagram. All three draw on AT ONCE.
    constexpr float kIntro = 0.15f;
    m_indicators.clear();
    m_indicators.push_back({ 'l', val(l), QColor(0xff, 0x6a, 0x3d),
        { hw, -hh,  hd }, { hw, -hh, -hd }, kIntro, 0.8f });
    m_indicators.push_back({ 'w', val(w), QColor(0x00, 0xe8, 0x7a),
        { -hw, -hh,  hd }, { hw, -hh,  hd }, kIntro, 0.8f });
    m_indicators.push_back({ 'h', val(h), QColor(0x4d, 0xa3, 0xff),
        { hw, -hh,  hd }, { hw,  hh,  hd }, kIntro, 0.8f });

    // -- Explanation terms: ONE face each, sharing the corner where the
    // l/w/h edges meet, so the persistent tinted faces form a coherent
    // half-shell for the "x2" duplication finale.
    auto cap = [](const char* term, float v) {
        return QString("%1 = %2").arg(QLatin1String(term))
            .arg(QString::number(v, 'g', 4));
        };

    m_faceTerms.clear();

    FaceTerm lw;   // bottom face: l (z) by w (x); duplicate slides up to the top
    lw.term = "lw"; lw.caption = cap("lw", l * w);
    lw.color = QColor(0xff, 0xd2, 0x4a); lw.group = 1;
    lw.quad[0] = { -hw, -hh, -hd }; lw.quad[1] = { hw, -hh, -hd };
    lw.quad[2] = { hw, -hh,  hd }; lw.quad[3] = { -hw, -hh,  hd };
    lw.center = { 0, -hh, 0 };
    m_faceTerms.push_back(lw);

    FaceTerm lh;   // right face: l (z) by h (y); duplicate slides to the left
    lh.term = "lh"; lh.caption = cap("lh", l * h);
    lh.color = QColor(0xc6, 0x78, 0xdd); lh.group = 2;
    lh.quad[0] = { hw, -hh, -hd }; lh.quad[1] = { hw, hh, -hd };
    lh.quad[2] = { hw,  hh,  hd }; lh.quad[3] = { hw, -hh,  hd };
    lh.center = { hw, 0, 0 };
    m_faceTerms.push_back(lh);

    FaceTerm wh;   // front face: w (x) by h (y); duplicate slides to the back
    wh.term = "wh"; wh.caption = cap("wh", w * h);
    wh.color = QColor(0x3d, 0xdb, 0xd9); wh.group = 3;
    wh.quad[0] = { -hw, -hh, hd }; wh.quad[1] = { hw, -hh, hd };
    wh.quad[2] = { hw,  hh, hd }; wh.quad[3] = { -hw, hh, hd };
    wh.center = { 0, 0, hd };
    m_faceTerms.push_back(wh);

    for (int i = 0; i < (int)m_faceTerms.size(); ++i) {
        m_faceTerms[i].tStart = kTermStart + i * kTermGap;
        m_faceTerms[i].tDur = kTermDur;
        m_faceTerms[i].dupStart = kDupStart;   // the shell moves as ONE block
    }

    // -- Cache every MathText layout ONCE here (re-laying-out per frame
    // transforms/allocates QPainterPaths and was a top per-frame cost).
    for (DimIndicator& d : m_indicators) {
        d.letterGlyphs = MathText::Line().run(QString(d.letter)).layout(44.0);
        d.valueGlyphs = MathText::Line().run(d.valueText).layout(19.0);
    }
    for (FaceTerm& t : m_faceTerms) {
        MathText::Line capLine;
        capLine.run(t.caption);
        t.captionGlyphs = capLine.layout(20.0);
        t.captionW = capLine.width(20.0);

        MathText::Line headLine;
        headLine.run(QStringLiteral("Term: ") + t.term);
        t.headlineGlyphs = headLine.layout(19.0);
        t.headlineW = headLine.width(19.0);
    }
    {
        MathText::Line h2;
        h2.run(QStringLiteral("Term: 2"));
        m_headline2Glyphs = h2.layout(19.0);
        m_headline2W = h2.width(19.0);
    }
}


float OpenGLRenderer::dupFlashIntensity() const {
    if (!m_animActive) return 0.0f;
    float f = 0.0f;
    for (const FaceTerm& ft : m_faceTerms) {
        const float p = animPhaseT(ft.dupStart, kDupDur);
        if (p <= 0.0f || p >= 1.0f) continue;
        // Bright the instant the duplicate departs, faded out by ~55% of the
        // slide — the fast-fading burst.
        const float k = 1.0f - std::clamp(p / 0.55f, 0.0f, 1.0f);
        f = std::max(f, k * k);
    }

    // Ending white-out: the scene's own glow swells as the white rises, so
    // the engulf reads as light, not a plain fade.
    const float pIn = animPhaseT(kWhiteStart, kWhiteIn);
    if (pIn > 0.0f && pIn < 1.0f)
        f = std::max(f, easeSmooth(pIn));

    return f;
}

void OpenGLRenderer::startFormulaAnimation(const QString& shapeType, const QString& /*formulaKey*/) {
    // Cuboid only for now; the indicator table is per-shape.
    if (shapeType != "cuboid" || m_shapeType != "cuboid" || !m_currentShape)
        return;

    rebuildIndicatorGeometry();

    // Readout split into colour groups: each term tints permanently as its
    // face builds, and the "2" (group 4) lights up for the duplication finale.
    m_readout = {
        { QStringLiteral("Surface Area = "), 0 },
        { QStringLiteral("2"), 4 },
        { QStringLiteral("("), 0 },
        { QStringLiteral("lw"), 1 },
        { QStringLiteral(" + "), 0 },
        { QStringLiteral("lh"), 2 },
        { QStringLiteral(" + "), 0 },
        { QStringLiteral("wh"), 3 },
        { QStringLiteral(")"), 0 },
    };

    // Cache the readout layout (static string; colours vary per frame but the
    // geometry never does).
    MathText::Line line;
    for (const ReadoutSeg& seg : m_readout)
        line.run(seg.text, seg.group);
    m_readoutGlyphs = line.layout(30.0);
    m_readoutW = line.width(30.0);

    // No camera pan and no rotation pause: the scene keeps living and the
    // annotations track it. Freezing everything made the walkthrough read
    // like a static image.
    m_animActive = true;
    m_animClock.start();
    update();
}

void OpenGLRenderer::stopFormulaAnimation() {
    if (!m_animActive) return;
    m_animActive = false;
    m_indicators.clear();
    m_faceTerms.clear();
    m_readout.clear();
    m_readoutGlyphs.clear();
    m_headline2Glyphs.clear();
    update();
}

// Colored edge highlights, drawn in world space at the end of drawScene().
// Each edge "grows" from a to b during its phase, then holds.
void OpenGLRenderer::drawAnimHighlights() {
    if (!m_animActive || m_indicators.empty()) return;
    // Once the ending white-out fully covers the screen, the walkthrough
    // visuals are gone — the fade-out must reveal the plain cuboid.
    if (animPhaseT(kWhiteFull, 0.001f) >= 1.0f) return;

    // Rebuild the fixed-function matrices explicitly: the glass path leaves
    // them set for its wireframe overlay, but don't depend on that.
    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -m_camDistance);
    view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);
    QMatrix4x4 proj;
    proj.perspective(45.0f, (float)width() / (float)height(), 0.1f, 100.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.constData());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.constData());

    // Highlights read as annotations, so they render on top of the shape.
    glDisable(GL_DEPTH_TEST);

    // -- Explanation faces (behind the edge highlights) -----------------
    // Each term builds ONE face — border traces itself, then the fill wipes
    // across — and PERSISTS. In the finale each built face is duplicated and
    // slides along its normal to the opposite side of the cuboid: the tinted
    // half-shell doubles into the complete surface (the "2").
    for (const FaceTerm& ft : m_faceTerms) {
        const float p = animPhaseT(ft.tStart, ft.tDur);   // holds at 1
        if (p <= 0.0f) continue;
        const QVector3D* q = ft.quad;

        // Border trace: walk the perimeter and emit bt of its length.
        const float bt = easeSmooth(std::clamp(p / kBorderFrac, 0.0f, 1.0f));
        float elen[4];
        float perimeter = 0.0f;
        for (int i = 0; i < 4; ++i) {
            elen[i] = (q[(i + 1) % 4] - q[i]).length();
            perimeter += elen[i];
        }
        float remain = perimeter * bt;
        glLineWidth(2.0f);
        glColor4f(ft.color.redF(), ft.color.greenF(), ft.color.blueF(), 0.9f);
        glBegin(GL_LINE_STRIP);
        glVertex3f(q[0].x(), q[0].y(), q[0].z());
        for (int i = 0; i < 4 && remain > 0.0f; ++i) {
            if (remain >= elen[i]) {
                const QVector3D& v = q[(i + 1) % 4];
                glVertex3f(v.x(), v.y(), v.z());
                remain -= elen[i];
            }
            else {
                const QVector3D v = q[i] + (q[(i + 1) % 4] - q[i]) * (remain / elen[i]);
                glVertex3f(v.x(), v.y(), v.z());
                remain = 0.0f;
            }
        }
        glEnd();

        // Fill wipe: the tint sweeps across from the q0-q1 edge — the visual
        // of the multiplication (one dimension swept along the other).
        const float fw = easeSmooth(
            std::clamp((p - kFillFrom) / kFillSpan, 0.0f, 1.0f));
        if (fw > 0.0f) {
            const QVector3D a = q[0] + (q[3] - q[0]) * fw;
            const QVector3D b = q[1] + (q[2] - q[1]) * fw;
            // 0.24 keeps the six-face end state from stacking toward white.
            glColor4f(ft.color.redF(), ft.color.greenF(), ft.color.blueF(), 0.24f);
            glBegin(GL_QUADS);
            glVertex3f(q[0].x(), q[0].y(), q[0].z());
            glVertex3f(q[1].x(), q[1].y(), q[1].z());
            glVertex3f(b.x(), b.y(), b.z());
            glVertex3f(a.x(), a.y(), a.z());
            glEnd();
        }

        // Finale: the whole duplicated shell swings around the box as ONE
        // rigid block (yaw + flip, see dupTransform) and settles onto the
        // opposite three faces. While its flash is live the block renders
        // WHITE-HOT — the bloom bright-pass wraps it in a fast-fading halo.
        const float dpRaw = animPhaseT(ft.dupStart, kDupDur);
        const float dp = easeSmooth(dpRaw);
        if (dp > 0.0f) {
            float flash = 1.0f - std::clamp(dpRaw / 0.55f, 0.0f, 1.0f);
            flash *= flash;
            auto hot = [flash](float c) { return c + (1.0f - c) * 0.35f * flash; };

            QVector3D tq[4];
            for (int i = 0; i < 4; ++i)
                tq[i] = dupTransform(q[i], dp);

            glColor4f(hot(ft.color.redF()), hot(ft.color.greenF()),
                hot(ft.color.blueF()), 0.24f + 0.25f * flash);
            glBegin(GL_QUADS);
            for (int i = 0; i < 4; ++i)
                glVertex3f(tq[i].x(), tq[i].y(), tq[i].z());
            glEnd();
            glLineWidth(2.0f + 1.5f * flash);
            glColor4f(hot(ft.color.redF()), hot(ft.color.greenF()),
                hot(ft.color.blueF()), 0.9f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 4; ++i)
                glVertex3f(tq[i].x(), tq[i].y(), tq[i].z());
            glEnd();
        }
    }

    for (const DimIndicator& d : m_indicators) {
        const float p = animPhaseT(d.tStart, d.tDur);
        if (p <= 0.0f) continue;

        const float grow = easeSmooth(p);
        const QVector3D tip = d.a + (d.b - d.a) * grow;
        const float alpha = std::min(1.0f, p * 2.0f);

        glLineWidth(5.0f);
        glColor4f(d.color.redF(), d.color.greenF(), d.color.blueF(), alpha);
        glBegin(GL_LINES);
        glVertex3f(d.a.x(), d.a.y(), d.a.z());
        glVertex3f(tip.x(), tip.y(), tip.z());
        glEnd();

        // End ticks once the edge has fully grown.
        if (p >= 1.0f) {
            glPointSize(7.0f);
            glBegin(GL_POINTS);
            glVertex3f(d.a.x(), d.a.y(), d.a.z());
            glVertex3f(d.b.x(), d.b.y(), d.b.z());
            glEnd();
        }
    }

    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

// Dimension annotations + the colored formula readout, rendered through
// MathText: every character is the source font's true vector outline, so
// glyphs write themselves on (outline trace -> fill), poster style — no
// chip/pill backgrounds. QPainter over the finished GL frame.
void OpenGLRenderer::drawAnimOverlay() {
    if (!m_animActive) return;

    // Paint the whole annotation layer with Qt's RASTER engine into an image,
    // then blit it over the GL frame. QPainter-on-GL antialiases paths far
    // worse than the raster engine — this is what makes the glyphs crisp.
    const qreal dpr = devicePixelRatioF();
    const QSize physical = size() * dpr;
    if (m_overlayImage.size() != physical) {
        m_overlayImage = QImage(physical, QImage::Format_ARGB32_Premultiplied);
        m_overlayImage.setDevicePixelRatio(dpr);
    }
    m_overlayImage.fill(Qt::transparent);

    QPainter painter(&m_overlayImage);
    painter.setRenderHint(QPainter::Antialiasing);

    // Past full white-out, the annotations no longer exist — the fade-out
    // reveals the plain cuboid.
    const bool hidden = animPhaseT(kWhiteFull, 0.001f) >= 1.0f;

    if (!hidden) for (const DimIndicator& d : m_indicators) {
        const float p = animPhaseT(d.tStart, d.tDur);
        if (p <= 0.0f) continue;

        // The label sits docked just outside its edge midpoint and WRITES
        // itself on in place (no fly-in), re-projected every frame so it
        // tracks the rotating shape.
        const QVector3D mid = (d.a + d.b) * 0.5f;
        const QPointF anchor = projectToScreen(mid);
        const QPointF pos = anchor + QPointF(26, -22);

        // Leader line letter -> edge midpoint once the label has arrived.
        if (p >= 1.0f) {
            QColor lc = d.color; lc.setAlpha(130);
            painter.setPen(QPen(lc, 1.2, Qt::DashLine));
            painter.drawLine(pos + QPointF(2, 4), anchor);
        }

        // Big letter writes itself on in place — pure white, poster style.
        // The colour link to its edge is carried by the highlighted edge +
        // leader line instead. (Layouts are cached; per-frame layout() was a
        // top CPU cost.)
        MathText::Style ls;
        ls.colorFor = [](int, QChar) { return QColor(255, 255, 255); };
        MathText::draw(painter, d.letterGlyphs, pos, ls, p);

        // Small "= value" annotation beneath, also pure white, written on
        // slightly after the letter.
        const qreal vt = std::clamp((p - 0.45f) / 0.55f, 0.0f, 1.0f);
        if (vt > 0.0) {
            MathText::Style vs;
            vs.colorFor = [](int, QChar) { return QColor(255, 255, 255); };
            vs.outlineWidth = 1.1;
            MathText::draw(painter, d.valueGlyphs,
                pos + QPointF(16.0, 24.0), vs, vt);
        }
    }

    // -- Explanation captions: the term's area product, written onto the
    // built face (persists), and again onto the duplicate as it lands.
    if (!hidden) for (const FaceTerm& ftm : m_faceTerms) {
        const float p = animPhaseT(ftm.tStart, ftm.tDur);   // holds at 1
        if (p <= 0.0f) continue;

        constexpr qreal em = 20.0;
        MathText::Style cs;
        const QColor cc = ftm.color;
        cs.colorFor = [&cc](int, QChar) { return cc; };
        cs.outlineWidth = 1.2;

        const qreal writeT = std::clamp((p - kCapFrom) / kCapSpan, 0.0f, 1.0f);
        if (writeT > 0.0) {
            const QPointF c = projectToScreen(ftm.center);
            MathText::draw(painter, ftm.captionGlyphs,
                c + QPointF(-ftm.captionW / 2.0, em * 0.35), cs, writeT);
        }

        // Caption rides the rotating block and writes on as it settles.
        const float dp = easeSmooth(animPhaseT(ftm.dupStart, kDupDur));
        if (dp > 0.0f) {
            const QPointF c = projectToScreen(dupTransform(ftm.center, dp));
            const qreal dwriteT = std::clamp((dp - 0.65f) / 0.35f, 0.0f, 1.0f);
            if (dwriteT > 0.0)
                MathText::draw(painter, ftm.captionGlyphs,
                    c + QPointF(-ftm.captionW / 2.0, em * 0.35), cs, dwriteT);
        }
    }

    // -- "Term: lw" headline under the readout: names the current step.
    // During the finale it becomes "Term: 2" while the duplicates slide.
    if (!hidden) {
        const QVector<MathText::Placed>* glyphs = nullptr;
        qreal   glyphsW = 0;
        QColor  termColor(255, 255, 255);
        float   termT = 0.0f;

        const float dupP = animPhaseT(kDupStart, kDupDur);
        if (dupP > 0.0f && !m_headline2Glyphs.isEmpty()) {
            glyphs = &m_headline2Glyphs;
            glyphsW = m_headline2W;
            termT = std::clamp(dupP / 0.25f, 0.0f, 1.0f);
        }
        else {
            for (const FaceTerm& ftm : m_faceTerms) {
                const float p = animPhaseT(ftm.tStart, ftm.tDur);
                if (p > 0.0f) {   // latest started term wins
                    glyphs = &ftm.headlineGlyphs;
                    glyphsW = ftm.headlineW;
                    termColor = ftm.color;
                    termT = std::clamp(p / 0.35f, 0.0f, 1.0f);
                }
            }
        }

        if (glyphs && termT > 0.0f) {
            MathText::Style ts;
            ts.colorFor = [&termColor](int, QChar) { return termColor; };
            ts.outlineWidth = 1.1;
            MathText::draw(painter, *glyphs,
                QPointF((width() - glyphsW) / 2.0, 140.0), ts, termT);
        }
    }

    // Formula readout, top-centre: writes itself on at the start. Each term
    // TINTS PERMANENTLY to its face colour as that face builds (mirroring the
    // persistent fills); the "2" flares white for the duplication finale
    // while everything else steps back.
    const float ft = animPhaseT(0.0f, 1.1f);
    if (!hidden && ft > 0.0f && !m_readoutGlyphs.isEmpty()) {
        // Per-group tint progress (holds at 1 — nothing fades back).
        float termTint[4] = { 0, 0, 0, 0 };   // index = group 1..3
        QColor termColor[4];
        for (const FaceTerm& ftm : m_faceTerms) {
            if (ftm.group < 1 || ftm.group > 3) continue;
            const float p = animPhaseT(ftm.tStart, ftm.tDur);
            termTint[ftm.group - 1] = easeSmooth(std::clamp(p / 0.5f, 0.0f, 1.0f));
            termColor[ftm.group - 1] = ftm.color;
        }
        const float dupT = easeSmooth(animPhaseT(kDupStart, 0.5f));

        auto mixColor = [](const QColor& from, const QColor& to, float t) {
            return QColor::fromRgbF(
                from.redF() + (to.redF() - from.redF()) * t,
                from.greenF() + (to.greenF() - from.greenF()) * t,
                from.blueF() + (to.blueF() - from.blueF()) * t,
                from.alphaF() + (to.alphaF() - from.alphaF()) * t);
            };

        MathText::Style fs;
        fs.colorFor = [this, &termTint, &termColor, dupT, &mixColor]
        (int group, QChar ch) {
            QColor base(238, 238, 238);
            for (const DimIndicator& d : m_indicators)
                if (ch == d.letter) { base = d.color; break; }

            if (group >= 1 && group <= 3)
                return mixColor(base, termColor[group - 1], termTint[group - 1]);

            if (group == 4)   // the "2": flares bright white for the finale
                return mixColor(base, QColor(255, 255, 255), dupT);

            // Plain text steps back a little while the "2" takes the stage.
            QColor dim = base;
            dim.setAlphaF(base.alphaF() * (1.0f - 0.45f * dupT));
            return dim;
            };

        const qreal x = (width() - m_readoutW) / 2.0;
        // Baseline sits below GeoModeWidget's floating shape-selector bar
        // (top bar bottoms out around y=55); keep clear of it.
        MathText::draw(painter, m_readoutGlyphs, QPointF(x, 108.0), fs, ft);
    }

    // -- Ending white-out: pure white engulfs the screen, holds a beat, then
    // fades away to reveal the cuboid as it was before the walkthrough.
    {
        const float pIn = animPhaseT(kWhiteStart, kWhiteIn);
        if (pIn > 0.0f) {
            float a = easeSmooth(pIn);
            const float pOut = animPhaseT(kWhiteFull + kWhiteHold, kWhiteOut);
            if (pOut > 0.0f)
                a = 1.0f - easeSmooth(pOut);
            if (a > 0.0f)
                painter.fillRect(QRectF(0, 0, width(), height()),
                    QColor(255, 255, 255, int(255.0f * a)));
        }
    }

    painter.end();

    // Blit the raster-rendered annotation layer over the GL frame.
    QPainter blit(this);
    blit.drawImage(QRectF(0, 0, width(), height()), m_overlayImage);
}

void OpenGLRenderer::drawGlass() {
    // -- Build matrices ---------------------------------------------
    QMatrix4x4 model;   // identity — shape is at origin
    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -m_camDistance);
    view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);

    QMatrix4x4 proj;
    proj.perspective(45.0f, (float)width() / (float)height(), 0.1f, 100.0f);

    QMatrix4x4 mv = view * model;
    QMatrix4x4 mvp = proj * mv;

    // -- Glass pass -------------------------------------------------
    m_glassShader.bind();
    m_glassShader.setUniformValue("mvp", mvp);
    m_glassShader.setUniformValue("modelView", mv);
    m_glassShader.setUniformValue("normalMatrix", mv.normalMatrix());
    m_glassShader.setUniformValue("shapeColor", QVector3D(m_colorR, m_colorG, m_colorB));
    m_glassShader.setUniformValue("opacity", 0.25f);

    glEnable(GL_CULL_FACE);

    // Back faces first — gives interior depth to the glass
    glCullFace(GL_FRONT);
    m_currentShape->drawShader(m_glassShader);

    // Front faces on top
    glCullFace(GL_BACK);
    m_currentShape->drawShader(m_glassShader);

    glDisable(GL_CULL_FACE);
    m_glassShader.release();

    // -- Wireframe overlay — legacy path, no shader -----------------
    // Must reset OpenGL matrix state since shader bypassed it
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.constData());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(mv.constData());

    // Curved shapes (sphere/cylinder/circle) skip the overlay — their
    // tessellation grid reads as ugly criss-cross lines, not real edges.
    if (!m_currentShape->isCurved()) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glColor4f(m_colorR, m_colorG, m_colorB, 0.25f); // subtle, mostly transparent
        glLineWidth(0.8f);
        m_currentShape->draw();
        glDisable(GL_POLYGON_OFFSET_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}
void OpenGLRenderer::drawLegacy() {
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -m_camDistance);
    glRotatef(m_xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yRot, 0.0f, 1.0f, 0.0f);

    switch (m_renderMode) {
    case RenderMode::Wireframe:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(m_colorR, m_colorG, m_colorB);
        glLineWidth(1.5f);
        m_currentShape->draw();
        break;

    case RenderMode::Solid:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor3f(m_colorR, m_colorG, m_colorB);
        m_currentShape->draw();
        break;

    case RenderMode::SolidWireframe:
        // Pass 1 — fill
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor3f(m_colorR * 0.3f, m_colorG * 0.3f, m_colorB * 0.3f);
        m_currentShape->draw();
        // Pass 2 — wireframe overlay (skipped for curved shapes, whose grid
        // lines are tessellation artefacts rather than real edges).
        if (!m_currentShape->isCurved()) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            glColor3f(m_colorR, m_colorG, m_colorB);
            glLineWidth(1.0f);
            m_currentShape->draw();
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
        break;

    case RenderMode::Glass:
        // fallback if shader failed to compile
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(m_colorR, m_colorG, m_colorB);
        m_currentShape->draw();
        break;
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void OpenGLRenderer::drawSphere(float radius, int slices, int stacks) {
    for (int i = 0; i <= stacks; ++i) {
        float lat0 = M_PI * (-0.5f + (float)(i - 1) / stacks);
        float lat1 = M_PI * (-0.5f + (float)i / stacks);
        float z0 = radius * sin(lat0);
        float z1 = radius * sin(lat1);
        float r0 = radius * cos(lat0);
        float r1 = radius * cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = 2 * M_PI * (float)j / slices;
            float x = cos(lng);
            float y = sin(lng);
            glVertex3f(x * r0, y * r0, z0);
            glVertex3f(x * r1, y * r1, z1);
        }
        glEnd();
    }
}

void OpenGLRenderer::drawCube(float size) {
    float half = size / 2.0f;
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-half, -half, half); glVertex3f(half, -half, half);
    glVertex3f(half, half, half); glVertex3f(-half, half, half);
    // Back
    glVertex3f(-half, -half, -half); glVertex3f(half, -half, -half);
    glVertex3f(half, half, -half); glVertex3f(-half, half, -half);
    // Left
    glVertex3f(-half, -half, -half); glVertex3f(-half, -half, half);
    glVertex3f(-half, half, half); glVertex3f(-half, half, -half);
    // Right
    glVertex3f(half, -half, -half); glVertex3f(half, -half, half);
    glVertex3f(half, half, half); glVertex3f(half, half, -half);
    // Top
    glVertex3f(-half, half, -half); glVertex3f(half, half, -half);
    glVertex3f(half, half, half); glVertex3f(-half, half, half);
    // Bottom
    glVertex3f(-half, -half, -half); glVertex3f(half, -half, -half);
    glVertex3f(half, -half, half); glVertex3f(-half, -half, half);
    glEnd();
}

void OpenGLRenderer::drawCylinder(float radius, float height, int segments) {
    float halfH = height / 2.0f;
    // Sides
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, -halfH, z);
        glVertex3f(x, halfH, z);
    }
    glEnd();
    // Top cap
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, halfH, 0);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, halfH, z);
    }
    glEnd();
    // Bottom cap
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, -halfH, 0);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, -halfH, z);
    }
    glEnd();
}
void OpenGLRenderer::setHorizontalOffset(int offsetPixels) {
    m_horizontalOffset = offsetPixels;
}