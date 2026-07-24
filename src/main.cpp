#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
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
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kSmallWidth = 115;
constexpr int kMediumWidth = 154;
constexpr int kLargeWidth = 192;
constexpr double kFrameAspect = 208.0 / 192.0;

struct Sequence {
    QString name;
    int frameIntervalMs;
};

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
            chooseNextState();
        });

        const int savedWidth = settings_.value(QStringLiteral("petWidth"), kMediumWidth).toInt();
        setPetWidth(std::clamp(savedWidth, kSmallWidth, kLargeWidth), false);
        restorePosition();
        setSequence(QStringLiteral("idle"), randomBetween(2500, 4500));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const auto sequenceIt = frames_.constFind(currentSequence_.name);
        if (sequenceIt == frames_.cend() || sequenceIt->isEmpty()) {
            return;
        }

        const QPixmap &frame = sequenceIt->at(frameIndex_ % sequenceIt->size());
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(rect(), frame);
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
                setSequence(QStringLiteral("waving"), 4 * 180);
            }
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            setSequence(QStringLiteral("jumping"), 5 * 130);
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

        menu.addAction(QStringLiteral("Saludar"), this, [this] {
            setSequence(QStringLiteral("waving"), 4 * 180);
        });
        menu.addAction(QStringLiteral("Saltar"), this, [this] {
            setSequence(QStringLiteral("jumping"), 5 * 130);
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
            {QStringLiteral("look-a"), 210},
            {QStringLiteral("look-b"), 210},
            {QStringLiteral("running-left"), 90},
            {QStringLiteral("running-right"), 90},
            {QStringLiteral("running"), 120},
            {QStringLiteral("waiting"), 220},
            {QStringLiteral("review"), 220},
            {QStringLiteral("failed"), 230},
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
            }
        }
    }

    void setSequence(const QString &name, int durationMs) {
        if (!frames_.contains(name)) {
            return;
        }

        currentSequence_.name = name;
        currentSequence_.frameIntervalMs = frameIntervals_.value(name, 180);
        frameIndex_ = 0;
        frameTimer_.start(currentSequence_.frameIntervalMs);

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

        frameIndex_ = (frameIndex_ + 1) % sequenceIt->size();

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
        if (roll < 48) {
            beginWalk();
        } else if (roll < 68) {
            setSequence(QStringLiteral("idle"), randomBetween(2500, 6000));
        } else if (roll < 76) {
            setSequence(QStringLiteral("look-a"), randomBetween(1700, 3000));
        } else if (roll < 84) {
            setSequence(QStringLiteral("look-b"), randomBetween(1700, 3000));
        } else if (roll < 90) {
            setSequence(QStringLiteral("waiting"), randomBetween(1600, 2800));
        } else if (roll < 94) {
            setSequence(QStringLiteral("review"), randomBetween(1500, 2600));
        } else if (roll < 98) {
            setSequence(QStringLiteral("waving"), 4 * 180);
        } else {
            setSequence(QStringLiteral("jumping"), 5 * 130);
        }
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
        resize(newWidth, static_cast<int>(std::round(newWidth * kFrameAspect)));
        groundBottom_ = oldBottom;
        if (isVisible()) {
            clampToCurrentScreen();
        }
        if (persist) {
            settings_.setValue(QStringLiteral("petWidth"), newWidth);
            savePosition();
        }
    }

    void addSizeAction(QMenu *menu, const QString &label, int width) {
        QAction *action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(this->width() == width);
        connect(action, &QAction::triggered, this, [this, width] {
            setPetWidth(width);
        });
    }

    void savePosition() {
        settings_.setValue(QStringLiteral("x"), x());
        settings_.setValue(QStringLiteral("groundBottom"), groundBottom_);
        settings_.setValue(QStringLiteral("petWidth"), width());
        settings_.sync();
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
            "X-KDE-autostart-after=panel\n")
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
    Sequence currentSequence_{QStringLiteral("idle"), 260};
    QTimer frameTimer_;
    QTimer stateTimer_;
    QSettings settings_;
    int frameIndex_ = 0;
    int groundBottom_ = 0;
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

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("planck-pet"));
    QCoreApplication::setOrganizationName(QStringLiteral("Carlos"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
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
