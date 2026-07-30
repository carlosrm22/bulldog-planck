#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QLockFile>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kSmallWidth = 115;
constexpr int kMediumWidth = 154;
constexpr int kLargeWidth = 192;
constexpr int kQuietPauseMinimumMilliseconds = 3000;
constexpr int kQuietPauseMaximumMilliseconds = 5000;
constexpr int kIdleMinimumMilliseconds = 4000;
constexpr int kIdleMaximumMilliseconds = 7000;
constexpr int kWaitingMinimumMilliseconds = 6000;
constexpr int kWaitingMaximumMilliseconds = 10000;
constexpr int kBlinkChancePercent = 12;
constexpr int kWaitingBlinkChancePercent = 20;
constexpr int kBlinkClosedMilliseconds = 120;
constexpr int kXWaylandStartupTimeoutMilliseconds = 30000;
constexpr int kXWaylandRetryMilliseconds = 250;
constexpr double kLateralVisualScale = 1.30;
constexpr double kJumpVisualScale = 1.20;
constexpr std::array<int, 5> kJumpFrameIntervals{
    170, 100, 110, 100, 170};
constexpr std::array<int, 4> kWaveFrameIntervals{
    180, 140, 250, 150};
constexpr std::array<int, 6> kReviewFrameIntervals{
    240, 180, 260, 180, 140, 320};
constexpr std::array<int, 9> kLookAFrameIntervals{
    300, 180, 150, 140, 130, 130, 140, 160, 420};
constexpr std::array<int, 9> kLookBFrameIntervals{
    300, 150, 150, 140, 130, 140, 150, 180, 400};
constexpr std::array<int, 8> kFailedFrameIntervals{
    230, 190, 170, 170, 18000, 230, 220, 290};
constexpr std::array<int, 17> kThinkingWorkFrameIntervals{
    100, 140, 140, 1600, 150, 1500,
    150, 2000, 150, 1500, 150, 1350,
    150, 1320, 140, 160, 100};
constexpr double kFrameAspect = 208.0 / 192.0;

struct Sequence {
    QString name;
    int frameIntervalMs;
    bool loops = true;
};

bool waitForXWayland() {
    QElapsedTimer timer;
    timer.start();
    do {
        QStringList displays;
        const QString inheritedDisplay = qEnvironmentVariable("DISPLAY");
        if (!inheritedDisplay.isEmpty()) {
            displays.append(inheritedDisplay);
        }

        const QDir socketDirectory(QStringLiteral("/tmp/.X11-unix"));
        const QStringList sockets = socketDirectory.entryList(
            QStringList{QStringLiteral("X*")},
            QDir::Files | QDir::System | QDir::NoDotAndDotDot,
            QDir::Name);
        for (const QString &socket : sockets) {
            bool isDisplayNumber = false;
            const int displayNumber = socket.sliced(1).toInt(&isDisplayNumber);
            if (!isDisplayNumber) {
                continue;
            }
            const QString display = QStringLiteral(":%1").arg(displayNumber);
            if (!displays.contains(display)) {
                displays.append(display);
            }
        }

        QStringList xAuthorities;
        const QString inheritedAuthority = qEnvironmentVariable("XAUTHORITY");
        if (!inheritedAuthority.isEmpty()) {
            xAuthorities.append(inheritedAuthority);
        }

        const QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (!runtimeDirectory.isEmpty()) {
            const QFileInfoList authorityFiles = QDir(runtimeDirectory).entryInfoList(
                QStringList{QStringLiteral("xauth_*")},
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Time);
            for (const QFileInfo &authorityFile : authorityFiles) {
                const QString authorityPath = authorityFile.absoluteFilePath();
                if (!xAuthorities.contains(authorityPath)) {
                    xAuthorities.append(authorityPath);
                }
            }
        }
        if (xAuthorities.isEmpty()) {
            xAuthorities.append(QString());
        }

        for (const QString &display : displays) {
            const QByteArray displayName = display.toLocal8Bit();
            for (const QString &authority : xAuthorities) {
                if (authority.isEmpty()) {
                    qunsetenv("XAUTHORITY");
                } else {
                    qputenv("XAUTHORITY", authority.toLocal8Bit());
                }

                int preferredScreen = 0;
                xcb_connection_t *connection =
                    xcb_connect(displayName.constData(), &preferredScreen);
                const bool connected = connection != nullptr
                    && xcb_connection_has_error(connection) == 0;
                if (connection != nullptr) {
                    xcb_disconnect(connection);
                }
                if (connected) {
                    qputenv("DISPLAY", displayName);
                    return true;
                }
            }
        }

        QThread::msleep(kXWaylandRetryMilliseconds);
    } while (timer.elapsed() < kXWaylandStartupTimeoutMilliseconds);

    return false;
}

class PetWindow final : public QWidget {
public:
    explicit PetWindow(QString framesRoot)
        : framesRoot_(std::move(framesRoot)),
          settings_(QStringLiteral("Carlos"), QStringLiteral("PlanckPet")) {
        setWindowTitle(QStringLiteral("Planck"));
        setWindowFlags(Qt::FramelessWindowHint
                       | Qt::WindowStaysOnTopHint
                       | Qt::Tool
                       | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        setToolTip(QStringLiteral("Planck · clic para saludar · arrastra para mover"));

        loadFrames();

        frameTimer_.setTimerType(Qt::PreciseTimer);
        connect(&frameTimer_, &QTimer::timeout, this, [this] {
            advanceFrame();
        });

        stateTimer_.setSingleShot(true);
        connect(&stateTimer_, &QTimer::timeout, this, [this] {
            finishCurrentState();
        });

        const int savedWidth = settings_.value(QStringLiteral("petWidth"), kMediumWidth).toInt();
        setPetWidth(std::clamp(savedWidth, kSmallWidth, kLargeWidth), false);
        restorePosition();
        beginQuietPause();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const auto sequenceIt = frames_.constFind(currentSequence_.name);
        if (sequenceIt == frames_.cend() || sequenceIt->isEmpty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF targetRect = frameTargetRect(currentSequence_.name);
        const QPixmap &currentFrame =
            sequenceIt->at(frameIndex_ % sequenceIt->size());
        painter.drawPixmap(targetRect, currentFrame, QRectF(currentFrame.rect()));
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            dragged_ = false;
            dragStartGlobal_ = event->globalPosition();
            dragStartWindow_ = pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!dragging_) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        const QPointF delta = event->globalPosition() - dragStartGlobal_;
        if (delta.manhattanLength() > QApplication::startDragDistance()) {
            dragged_ = true;
        }
        move(dragStartWindow_ + delta.toPoint());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            groundBottom_ = y() + height();
            clampToCurrentScreen();
            savePosition();
            if (!dragged_) {
                setSequence(
                    QStringLiteral("waving"),
                    durationForCycles(QStringLiteral("waving"), 1));
            }
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            setSequence(
                QStringLiteral("jumping"),
                durationForCycles(QStringLiteral("jumping"), 1));
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);

        QAction *pauseAction = menu.addAction(paused_
            ? QStringLiteral("Continuar caminando")
            : QStringLiteral("Pausar"));
        pauseAction->setCheckable(true);
        pauseAction->setChecked(paused_);

        QMenu *actionsMenu = menu.addMenu(QStringLiteral("Acciones"));
        actionsMenu->addAction(QStringLiteral("Saludar"), this, [this] {
            setSequence(
                QStringLiteral("waving"),
                durationForCycles(QStringLiteral("waving"), 1));
        });
        actionsMenu->addAction(QStringLiteral("Saltar"), this, [this] {
            setSequence(
                QStringLiteral("jumping"),
                durationForCycles(QStringLiteral("jumping"), 1));
        });
        actionsMenu->addSeparator();
        actionsMenu->addAction(QStringLiteral("Revisar"), this, [this] {
            setSequence(
                QStringLiteral("review"),
                durationForCycles(QStringLiteral("review"), 2));
        });
        actionsMenu->addAction(QStringLiteral("Pensar / trabajar"), this, [this] {
            setSequence(
                QStringLiteral("thinking-work"),
                durationForCycles(QStringLiteral("thinking-work"), 1));
        });
        actionsMenu->addAction(QStringLiteral("Correr hacia ti"), this, [this] {
            setSequence(
                QStringLiteral("running"),
                durationForCycles(QStringLiteral("running"), 3));
        });
        actionsMenu->addAction(QStringLiteral("Tumbarse y descansar"), this, [this] {
            setSequence(
                QStringLiteral("failed"),
                durationForCycles(QStringLiteral("failed"), 1));
        });

        QMenu *sizeMenu = menu.addMenu(QStringLiteral("Tamaño"));
        addSizeAction(sizeMenu, QStringLiteral("Pequeño"), kSmallWidth);
        addSizeAction(sizeMenu, QStringLiteral("Mediano"), kMediumWidth);
        addSizeAction(sizeMenu, QStringLiteral("Grande"), kLargeWidth);

        menu.addSeparator();
        menu.addAction(QStringLiteral("Colocar abajo"), this, [this] {
            snapToBottom();
        });

        QAction *autostartAction = menu.addAction(QStringLiteral("Iniciar con la sesión"));
        autostartAction->setCheckable(true);
        autostartAction->setChecked(autostartEnabled());

        menu.addSeparator();
        menu.addAction(QStringLiteral("Salir"), qApp, &QApplication::quit);

        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == pauseAction) {
            paused_ = pauseAction->isChecked();
            if (paused_) {
                setSequence(QStringLiteral("idle"), 0);
            } else {
                chooseNextState();
            }
        } else if (chosen == autostartAction) {
            setAutostart(autostartAction->isChecked());
        }
    }

    void closeEvent(QCloseEvent *event) override {
        savePosition();
        event->accept();
        QApplication::quit();
    }

private:
    void loadFrames() {
        const QList<Sequence> sequences{
            {QStringLiteral("idle"), 260},
            {QStringLiteral("look-a"), 210, true},
            {QStringLiteral("look-b"), 210, true},
            {QStringLiteral("running-left"), 90},
            {QStringLiteral("running-right"), 90},
            {QStringLiteral("running"), 120},
            {QStringLiteral("waiting"), 220},
            {QStringLiteral("review"), 220},
            {QStringLiteral("thinking-work"), 1800, false},
            {QStringLiteral("failed"), 230, false},
            {QStringLiteral("waving"), 180},
            {QStringLiteral("jumping"), 130},
        };

        for (const Sequence &sequence : sequences) {
            QDir dir(QDir(framesRoot_).filePath(sequence.name));
            const QStringList names = dir.entryList(
                {QStringLiteral("*.png")}, QDir::Files, QDir::Name);

            QVector<QPixmap> pixmaps;
            pixmaps.reserve(names.size());
            for (const QString &name : names) {
                QPixmap pixmap(dir.filePath(name));
                if (!pixmap.isNull()) {
                    pixmaps.push_back(std::move(pixmap));
                }
            }

            if (!pixmaps.isEmpty()) {
                frames_.insert(sequence.name, std::move(pixmaps));
                frameIntervals_.insert(sequence.name, sequence.frameIntervalMs);
                sequenceLoops_.insert(sequence.name, sequence.loops);
            }
        }

        pingPongSequences_.insert(QStringLiteral("look-a"));
        pingPongSequences_.insert(QStringLiteral("look-b"));
    }

    int intervalForFrame(const QString &name, int frameIndex) const {
        if (name == QStringLiteral("jumping")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kJumpFrameIntervals.size())) {
            return kJumpFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("waving")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kWaveFrameIntervals.size())) {
            return kWaveFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("review")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kReviewFrameIntervals.size())) {
            return kReviewFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("look-a")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kLookAFrameIntervals.size())) {
            return kLookAFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("look-b")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kLookBFrameIntervals.size())) {
            return kLookBFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("failed")
            && frameIndex >= 0
            && frameIndex < static_cast<int>(kFailedFrameIntervals.size())) {
            return kFailedFrameIntervals.at(frameIndex);
        }
        if (name == QStringLiteral("thinking-work")
            && frameIndex >= 0
            && frameIndex
                < static_cast<int>(kThinkingWorkFrameIntervals.size())) {
            return kThinkingWorkFrameIntervals.at(frameIndex);
        }

        const bool isBlinkingSequence =
            name == QStringLiteral("idle")
            || name == QStringLiteral("waiting");
        if (isBlinkingSequence && frameIndex == 2) {
            return kBlinkClosedMilliseconds;
        }

        return frameIntervals_.value(name, 180);
    }

    void setSequence(const QString &name, int durationMs) {
        if (!frames_.contains(name)) {
            return;
        }

        currentSequence_.name = name;
        currentSequence_.frameIntervalMs = frameIntervals_.value(name, 180);
        applySequenceGeometry(name);
        frameIndex_ = 0;
        frameDirection_ = 1;
        frameTimer_.start(intervalForFrame(name, frameIndex_));

        if (durationMs > 0) {
            stateTimer_.start(durationMs);
        } else {
            stateTimer_.stop();
        }
        update();
    }

    void advanceFrame() {
        const auto sequenceIt = frames_.constFind(currentSequence_.name);
        if (sequenceIt == frames_.cend() || sequenceIt->isEmpty()) {
            return;
        }

        const int frameCount = sequenceIt->size();
        const int lastFrame = frameCount - 1;
        int nextFrame = frameIndex_;
        if (pingPongSequences_.contains(currentSequence_.name)
            && frameCount > 1) {
            nextFrame = frameIndex_ + frameDirection_;
            if (nextFrame >= frameCount) {
                frameDirection_ = -1;
                nextFrame = lastFrame - 1;
            } else if (nextFrame < 0) {
                frameDirection_ = 1;
                nextFrame = 1;
            }
        } else if (frameIndex_ < lastFrame) {
            nextFrame = frameIndex_ + 1;
        } else if (sequenceLoops_.value(currentSequence_.name, true)) {
            nextFrame = 0;
        }

        const bool isIdle = currentSequence_.name == QStringLiteral("idle");
        const bool isWaiting = currentSequence_.name == QStringLiteral("waiting");
        const bool isBlinkingSequence = isIdle || isWaiting;
        const bool wouldBlink = isBlinkingSequence && nextFrame == 2;
        const int blinkChancePercent =
            isWaiting ? kWaitingBlinkChancePercent : kBlinkChancePercent;
        if (wouldBlink
            && QRandomGenerator::global()->bounded(100) >= blinkChancePercent) {
            nextFrame = 3;
        }

        if (nextFrame != frameIndex_) {
            frameIndex_ = nextFrame;
        }

        const int nextFrameInterval =
            intervalForFrame(currentSequence_.name, frameIndex_);
        if (frameTimer_.interval() != nextFrameInterval) {
            frameTimer_.setInterval(nextFrameInterval);
        }

        if (!paused_ && !dragging_) {
            if (currentSequence_.name == QStringLiteral("running-left")) {
                moveHorizontally(-movementStep_);
            } else if (currentSequence_.name == QStringLiteral("running-right")) {
                moveHorizontally(movementStep_);
            }
        }
        update();
    }

    void chooseNextState() {
        if (paused_) {
            setSequence(QStringLiteral("idle"), 0);
            return;
        }

        const int roll = randomBetween(0, 99);
        if (roll < 28) {
            beginWalk();
        } else if (roll < 32) {
            setSequence(
                QStringLiteral("idle"),
                randomBetween(kIdleMinimumMilliseconds, kIdleMaximumMilliseconds));
        } else if (roll < 44) {
            setSequence(
                QStringLiteral("waving"),
                durationForCycles(QStringLiteral("waving"), 1));
        } else if (roll < 50) {
            setSequence(
                QStringLiteral("look-a"),
                durationForPingPongCycles(QStringLiteral("look-a"), 1));
        } else if (roll < 56) {
            setSequence(
                QStringLiteral("look-b"),
                durationForPingPongCycles(QStringLiteral("look-b"), 1));
        } else if (roll < 70) {
            setSequence(
                QStringLiteral("waiting"),
                randomBetween(kWaitingMinimumMilliseconds, kWaitingMaximumMilliseconds));
        } else if (roll < 77) {
            setSequence(
                QStringLiteral("review"),
                durationForCycles(QStringLiteral("review"), 2));
        } else if (roll < 86) {
            setSequence(
                QStringLiteral("thinking-work"),
                durationForCycles(QStringLiteral("thinking-work"), 1));
        } else if (roll < 91) {
            setSequence(
                QStringLiteral("running"),
                durationForCycles(QStringLiteral("running"), randomBetween(2, 4)));
        } else if (roll < 96) {
            setSequence(
                QStringLiteral("failed"),
                durationForCycles(QStringLiteral("failed"), 1));
        } else {
            setSequence(
                QStringLiteral("jumping"),
                durationForCycles(QStringLiteral("jumping"), 1));
        }
    }

    void finishCurrentState() {
        if (paused_) {
            setSequence(QStringLiteral("idle"), 0);
            return;
        }

        if (currentSequence_.name == QStringLiteral("idle")) {
            chooseNextState();
            return;
        }

        beginQuietPause();
    }

    void beginQuietPause() {
        setSequence(
            QStringLiteral("idle"),
            randomBetween(
                kQuietPauseMinimumMilliseconds,
                kQuietPauseMaximumMilliseconds));
    }

    int durationForCycles(const QString &name, int cycles) const {
        const int frameCount = frames_.value(name).size();
        int cycleDuration = 0;
        for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
            cycleDuration += intervalForFrame(name, frameIndex);
        }
        return cycleDuration * std::max(1, cycles);
    }

    int durationForPingPongCycles(const QString &name, int cycles) const {
        const int frameCount = frames_.value(name).size();
        if (frameCount == 0) {
            return 0;
        }

        int cycleDuration = intervalForFrame(name, 0);
        if (frameCount > 1) {
            cycleDuration += intervalForFrame(name, frameCount - 1);
            for (int frameIndex = 1;
                 frameIndex < frameCount - 1;
                 ++frameIndex) {
                cycleDuration += 2 * intervalForFrame(name, frameIndex);
            }
        }

        return cycleDuration * std::max(1, cycles);
    }

    void beginWalk() {
        const QRect bounds = currentScreenGeometry();
        const int leftRoom = x() - bounds.left();
        const int rightRoom = bounds.right() - (x() + width());

        bool goRight = QRandomGenerator::global()->bounded(2) == 1;
        if (leftRoom < width()) {
            goRight = true;
        } else if (rightRoom < width()) {
            goRight = false;
        }

        setSequence(
            goRight ? QStringLiteral("running-right") : QStringLiteral("running-left"),
            randomBetween(2600, 7200));
    }

    void moveHorizontally(int delta) {
        const QRect bounds = currentScreenGeometry();
        int nextX = x() + delta;

        if (nextX <= bounds.left()) {
            nextX = bounds.left();
            move(nextX, groundBottom_ - height());
            setSequence(QStringLiteral("running-right"), randomBetween(2200, 5200));
            return;
        }

        const int maximumX = bounds.right() - width() + 1;
        if (nextX >= maximumX) {
            nextX = maximumX;
            move(nextX, groundBottom_ - height());
            setSequence(QStringLiteral("running-left"), randomBetween(2200, 5200));
            return;
        }

        move(nextX, groundBottom_ - height());
    }

    QRect currentScreenGeometry() const {
        QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        return screen->availableGeometry();
    }

    void restorePosition() {
        const QRect bounds = QGuiApplication::primaryScreen()->availableGeometry();
        const bool hasPosition = settings_.contains(QStringLiteral("x"))
            && settings_.contains(QStringLiteral("groundBottom"));

        int restoredX = bounds.center().x() - width() / 2;
        groundBottom_ = bounds.bottom() + 1;
        if (hasPosition) {
            restoredX = settings_.value(QStringLiteral("x")).toInt();
            groundBottom_ = settings_.value(QStringLiteral("groundBottom")).toInt();
        }

        move(restoredX, groundBottom_ - height());
        clampToCurrentScreen();
    }

    void clampToCurrentScreen() {
        const QRect bounds = currentScreenGeometry();
        const int clampedX = std::clamp(
            x(), bounds.left(), std::max(bounds.left(), bounds.right() - width() + 1));
        const int minimumBottom = bounds.top() + height();
        const int maximumBottom = bounds.bottom() + 1;
        groundBottom_ = std::clamp(groundBottom_, minimumBottom, maximumBottom);
        move(clampedX, groundBottom_ - height());
    }

    void snapToBottom() {
        const QRect bounds = currentScreenGeometry();
        groundBottom_ = bounds.bottom() + 1;
        clampToCurrentScreen();
        savePosition();
    }

    void setPetWidth(int newWidth, bool persist = true) {
        const int oldBottom = groundBottom_ > 0 ? groundBottom_ : y() + height();
        basePetWidth_ = newWidth;
        groundBottom_ = oldBottom;
        applySequenceGeometry(currentSequence_.name);
        if (persist) {
            savePosition();
        }
    }

    void addSizeAction(QMenu *menu, const QString &label, int width) {
        QAction *action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(basePetWidth_ == width);
        connect(action, &QAction::triggered, this, [this, width] {
            setPetWidth(width);
        });
    }

    void savePosition() {
        const int normalWidthOffset = (width() - basePetWidth_) / 2;
        settings_.setValue(QStringLiteral("x"), x() + normalWidthOffset);
        settings_.setValue(QStringLiteral("groundBottom"), groundBottom_);
        settings_.setValue(QStringLiteral("petWidth"), basePetWidth_);
        settings_.sync();
    }

    static bool isLateralSequence(const QString &name) {
        return name == QStringLiteral("running-left")
            || name == QStringLiteral("running-right");
    }

    static bool isJumpSequence(const QString &name) {
        return name == QStringLiteral("jumping");
    }

    static double windowScaleForSequence(const QString &name) {
        if (isLateralSequence(name)) {
            return kLateralVisualScale;
        }
        if (isJumpSequence(name)) {
            return kJumpVisualScale;
        }
        return 1.0;
    }

    QRectF frameTargetRect(const QString &name) const {
        if (!isLateralSequence(name)) {
            return QRectF(rect());
        }

        const qreal scaledHeight = height() * kLateralVisualScale;
        return QRectF(
            0.0,
            height() - scaledHeight,
            width(),
            scaledHeight);
    }

    void applySequenceGeometry(const QString &name) {
        const int oldCenterX = x() + width() / 2;
        const double windowScale = windowScaleForSequence(name);
        const int newWidth = static_cast<int>(std::round(
            basePetWidth_ * windowScale));
        const double heightScale = isJumpSequence(name) ? windowScale : 1.0;
        const int newHeight = static_cast<int>(std::round(
            basePetWidth_ * kFrameAspect * heightScale));

        resize(newWidth, newHeight);
        move(oldCenterX - newWidth / 2, groundBottom_ - newHeight);
        if (isVisible()) {
            clampToCurrentScreen();
        }
    }

    QString autostartPath() const {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .filePath(QStringLiteral("autostart/planck-pet.desktop"));
    }

    bool autostartEnabled() const {
        return QFileInfo::exists(autostartPath());
    }

    void setAutostart(bool enabled) {
        const QString path = autostartPath();
        if (!enabled) {
            QFile::remove(path);
            return;
        }

        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return;
        }

        QString executable = QCoreApplication::applicationFilePath();
        const QByteArray contents = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Planck\n"
            "Comment=Mascota animada para el escritorio\n"
            "Exec=\"%1\"\n"
            "Icon=planck-pet\n"
            "Terminal=false\n"
            "X-KDE-autostart-phase=2\n")
            .arg(executable.replace(QStringLiteral("\""), QStringLiteral("\\\"")))
            .toUtf8();
        file.write(contents);
    }

    static int randomBetween(int minimum, int maximum) {
        return QRandomGenerator::global()->bounded(minimum, maximum + 1);
    }

    QString framesRoot_;
    QHash<QString, QVector<QPixmap>> frames_;
    QHash<QString, int> frameIntervals_;
    QHash<QString, bool> sequenceLoops_;
    QSet<QString> pingPongSequences_;
    Sequence currentSequence_{QStringLiteral("idle"), 260};
    QTimer frameTimer_;
    QTimer stateTimer_;
    QSettings settings_;
    int frameIndex_ = 0;
    int frameDirection_ = 1;
    int groundBottom_ = 0;
    int basePetWidth_ = kMediumWidth;
    int movementStep_ = 4;
    bool paused_ = false;
    bool dragging_ = false;
    bool dragged_ = false;
    QPointF dragStartGlobal_;
    QPoint dragStartWindow_;
};

QString findFramesRoot() {
    const QString overridePath = qEnvironmentVariable("PLANCK_PET_FRAMES");
    if (!overridePath.isEmpty() && QDir(overridePath).exists()) {
        return overridePath;
    }

    const QString installed = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("planck-pet/frames"),
        QStandardPaths::LocateDirectory);
    if (!installed.isEmpty()) {
        return installed;
    }

    const QString besideExecutable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("../share/planck-pet/frames"));
    if (QDir(besideExecutable).exists()) {
        return QDir(besideExecutable).absolutePath();
    }

    return {};
}

} // namespace

int main(int argc, char *argv[]) {
    // KWin/Wayland intentionally restricts clients from moving top-level windows.
    // XWayland gives this small, transparent desktop companion predictable movement.
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    if (!waitForXWayland()) {
        qCritical("Planck could not connect to XWayland after %d ms.",
                  kXWaylandStartupTimeoutMilliseconds);
        return 3;
    }

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("planck-pet"));
    QCoreApplication::setOrganizationName(QStringLiteral("Carlos"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PLANCK_VERSION));
    app.setQuitOnLastWindowClosed(true);

    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QLockFile lock(QDir(runtimeDir).filePath(QStringLiteral("planck-pet.lock")));
    lock.setStaleLockTime(5000);
    if (!lock.tryLock(50)) {
        qint64 ownerPid = 0;
        QString ownerHost;
        QString ownerApplication;
        const bool hasOwner = lock.getLockInfo(
            &ownerPid, &ownerHost, &ownerApplication);
        const bool ownerIsRunning = hasOwner
            && QFileInfo::exists(QStringLiteral("/proc/%1").arg(ownerPid));
        if (ownerIsRunning || !lock.removeStaleLockFile() || !lock.tryLock(50)) {
            return 0;
        }
    }

    const QString framesRoot = findFramesRoot();
    if (framesRoot.isEmpty()) {
        return 2;
    }

    const QString iconPath = QDir(framesRoot).filePath(QStringLiteral("idle/idle-01.png"));
    app.setWindowIcon(QIcon(iconPath));

    PetWindow pet(framesRoot);
    pet.show();
    pet.raise();

    return app.exec();
}
