#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPainter>
#include <QDir>
#include <QDebug>
#include <QStringList>
#include <QRegExp>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QContextMenuEvent>
#include <QApplication>
#include <algorithm>
#include <malloc.h>
#include <unistd.h>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , timer(new QTimer(this))
    , stateTimer(new QTimer(this))
    , currentFrames(nullptr)
    , frameIndex(0)
    , currentState(PetState::SayHello)
    , stateElapsedMs(0)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
    , actionLaunchAgent(nullptr)
    , actionFreeMemory(nullptr)
    , actionAbout(nullptr)
    , actionExit(nullptr)
{
    ui->setupUi(this);

    resize(220, 220);

    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint
    );

    setAttribute(Qt::WA_TranslucentBackground);

    // Remove the resize grip (triangle) from the status bar
    ui->statusbar->setSizeGripEnabled(false);

    // Load all animation frames
    loadAnimationFrames();

    timer->setTimerType(Qt::PreciseTimer);

    // Animation playback timer (frame update)
    connect(timer, &QTimer::timeout, this, [this]() {
        if (!currentFrames || currentFrames->isEmpty())
            return;

        const bool loopCompleted = (frameIndex == currentFrames->size() - 1);
        frameIndex = (frameIndex + 1) % currentFrames->size();

        if (currentState == PetState::SayHello && loopCompleted)
        {
            changeState(PetState::Swing);
            return;
        }

        update();
    });
    timer->start(60);  // 40ms per frame = 25fps

    // State transition timer (check for state changes)
    connect(stateTimer, &QTimer::timeout, this, [this]() {
        stateElapsedMs += 100;  // Timer fires every 100ms

        if (currentState == PetState::Swing)
        {
            // After 2 minutes of swing, switch to sleep
            if (stateElapsedMs >= 120000)  // 120000ms = 2 minutes
            {
                changeState(PetState::Sleep);
                stateElapsedMs = 0;
            }
        }
    });
    stateTimer->start(100);  // Check state every 100ms

    // Start with SayHello animation
    changeState(PetState::SayHello);

    // Setup system tray icon and context menu
    setupTrayIcon();
}

void MainWindow::loadAnimationFrames()
{
    auto loadFramesFromDir = [this](const QString &relDirPath, QVector<QPixmap> &targetFrames) {
        const QString fullPath = QDir(PROJECT_ROOT).filePath(relDirPath);
        QDir dir(fullPath);
        QStringList filters;
        filters << "*.png";

        QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

        // Natural number sort: extract numbers from filenames
        std::sort(files.begin(), files.end(),
            [](const QString &a, const QString &b) {
                auto getNumber = [](const QString &str) -> int {
                    QRegExp rx("_(\\d+)\\.png");
                    if (rx.indexIn(str) != -1) {
                        return rx.cap(1).toInt();
                    }
                    return -1;
                };
                return getNumber(a) < getNumber(b);
            });

        for (const QString &file : files)
        {
            QPixmap pix;
            bool ok = pix.load(dir.absoluteFilePath(file));

            if (ok)
            {
                // Pre-scale to window size, keeping aspect ratio
                QPixmap scaledPix = pix.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                targetFrames.push_back(scaledPix);
            }
            else
            {
                qDebug() << "Failed to load:" << dir.absoluteFilePath(file);
            }
        }
    };

    loadFramesFromDir("img/sayHello", sayHelloFrames);
    loadFramesFromDir("img/swing", swingFrames);
    loadFramesFromDir("img/sleep", sleepFrames);
    smoothSwingAnimation();

    qDebug() << "Project root:" << PROJECT_ROOT;
    qDebug() << "Loaded frames:"
             << "SayHello:" << sayHelloFrames.size()
             << "Swing:" << swingFrames.size()
             << "Sleep:" << sleepFrames.size();
}

void MainWindow::smoothSwingAnimation()
{
    if (swingFrames.size() < 18)
        return;

    QVector<QPixmap> smoothedFrames;
    const QVector<int> forwardOrder = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        12, 13, 14, 15, 16, 17, 0
    };

    for (int index : forwardOrder)
    {
        smoothedFrames.push_back(swingFrames[index]);
    }

    for (int i = forwardOrder.size() - 2; i >= 0; --i)
    {
        smoothedFrames.push_back(swingFrames[forwardOrder[i]]);
    }

    swingFrames = smoothedFrames;
}

void MainWindow::changeState(PetState newState)
{
    currentState = newState;
    frameIndex = 0;
    stateElapsedMs = 0;

    switch (newState)
    {
        case PetState::SayHello:
            currentFrames = &sayHelloFrames;
            qDebug() << "Changed to SayHello state";
            break;
        case PetState::Swing:
            currentFrames = &swingFrames;
            qDebug() << "Changed to Swing state";
            break;
        case PetState::Sleep:
            currentFrames = &sleepFrames;
            qDebug() << "Changed to Sleep state";
            break;
    }

    update();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    if (currentFrames && !currentFrames->isEmpty())
    {
        painter.drawPixmap(0, 0, (*currentFrames)[frameIndex]);
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        dragPosition = event->globalPos() - frameGeometry().topLeft();

        // Any click triggers SayHello animation
        changeState(PetState::SayHello);
        event->accept();
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);
    if (!trayMenu)
        return;

    // Show menu to the right of the pet window, not on top of it
    const int margin = 8;
    QPoint menuPos = mapToGlobal(QPoint(width() + margin, 0));

    // If not enough space on the right, show below
    QScreen *sc = screen();
    if (sc && menuPos.x() + trayMenu->sizeHint().width() > sc->availableGeometry().width())
    {
        menuPos = mapToGlobal(QPoint(0, height() + margin));
    }

    trayMenu->exec(menuPos);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && currentState != PetState::Sleep)
    {
        move(event->globalPos() - dragPosition);
        event->accept();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Minimize to tray instead of quitting
    if (trayIcon && trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
    else
    {
        event->accept();
    }
}

void MainWindow::setupTrayIcon()
{
    const QString iconPath = QDir(PROJECT_ROOT).filePath("img/icon.png");
    QIcon icon(iconPath);

    if (icon.isNull())
    {
        qDebug() << "Failed to load tray icon:" << iconPath;
        return;
    }

    // Create actions
    actionLaunchAgent = new QAction(QIcon::fromTheme("utilities-terminal"), "🖥️ 唤起 Agent", this);
    actionFreeMemory  = new QAction(QIcon::fromTheme("edit-clear"), "🧹 释放内存", this);
    actionAbout       = new QAction(QIcon::fromTheme("help-about"), "ℹ️ 关于", this);
    actionExit        = new QAction(QIcon::fromTheme("application-exit"), "❌ 退出", this);

    // Create menu with cute green style
    trayMenu = new QMenu(this);
    trayMenu->setWindowFlags(trayMenu->windowFlags() | Qt::FramelessWindowHint);
    trayMenu->setAttribute(Qt::WA_TranslucentBackground);
    trayMenu->setStyleSheet(
        "QMenu {"
        "  background-color: #f0faf0;"
        "  border: 2px solid #66bb6a;"
        "  border-radius: 14px;"
        "  padding: 8px 0px;"
        "}"
        "QMenu::item {"
        "  padding: 9px 30px 9px 18px;"
        "  margin: 3px 8px;"
        "  border-radius: 9px;"
        "  color: #2e7d32;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QMenu::item:selected {"
        "  background: #81c784;"
        "  color: #ffffff;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background-color: #c8e6c9;"
        "  margin: 5px 14px;"
        "}"
    );
    trayMenu->addAction(actionLaunchAgent);
    trayMenu->addAction(actionFreeMemory);
    trayMenu->addSeparator();
    trayMenu->addAction(actionAbout);
    trayMenu->addSeparator();
    trayMenu->addAction(actionExit);

    // Create tray icon
    trayIcon = new QSystemTrayIcon(icon, this);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setToolTip("桌面宠物");

    // Connect actions
    connect(actionLaunchAgent, &QAction::triggered, this, &MainWindow::launchAgent);
    connect(actionFreeMemory,  &QAction::triggered, this, &MainWindow::releaseSystemMemory);
    connect(actionAbout,       &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(actionExit,        &QAction::triggered, qApp, &QApplication::quit);

    // Left-click on tray icon toggles window visibility
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
        {
            if (isVisible())
            {
                hide();
            }
            else
            {
                show();
                raise();
                activateWindow();
            }
        }
    });

    trayIcon->show();
    qDebug() << "System tray icon set up successfully";
}

void MainWindow::launchAgent()
{
    // Try to find an available terminal emulator
    QStringList terminals = {
        "x-terminal-emulator",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "lxterminal",
        "xterm"
    };

    QString terminal;
    for (const auto &t : terminals)
    {
        terminal = QStandardPaths::findExecutable(t);
        if (!terminal.isEmpty())
            break;
    }

    if (terminal.isEmpty())
    {
        // No terminal found, run deepy directly (headless)
        QProcess::startDetached("deepy", {"tui"});
        trayIcon->showMessage("唤起 Agent", "未找到终端模拟器，已在后台启动 deepy",
                              QSystemTrayIcon::Warning, 3000);
        return;
    }

    // Launch deepy tui in the terminal
    bool started = QProcess::startDetached(terminal, {"-e", "deepy tui"});

    if (started)
    {
        trayIcon->showMessage("唤起 Agent", "已在终端中启动 deepy tui",
                              QSystemTrayIcon::Information, 2000);
    }
    else
    {
        trayIcon->showMessage("唤起 Agent", "启动失败，请检查 deepy 是否已安装",
                              QSystemTrayIcon::Critical, 3000);
    }
}

void MainWindow::releaseSystemMemory()
{
    sync();           // Flush filesystem buffers
    malloc_trim(0);   // Release freed heap memory back to OS

    trayIcon->showMessage("释放内存", "系统内存已成功释放！",
                          QSystemTrayIcon::Information, 3000);
    qDebug() << "System memory released via malloc_trim(0)";
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "关于 桌面宠物",
        "<h3>📌 桌面宠物 v1.0</h3>"
        "<p>基于 Qt5 开发的桌面伙伴</p>"
        "<hr>"
        "<p><b>动画状态:</b></p>"
        "<ul>"
        "<li>👋 SayHello - 打招呼</li>"
        "<li>🎠 Swing - 摇摆</li>"
        "<li>😴 Sleep - 睡觉</li>"
        "</ul>"
        "<hr>"
        "<p><b>交互方式:</b></p>"
        "<ul>"
        "<li>点击宠物 → 打招呼</li>"
        "<li>拖拽移动（睡眠时不可拖拽）</li>"
        "<li>托盘右键菜单 → 更多功能</li>"
        "</ul>"
    );
}
