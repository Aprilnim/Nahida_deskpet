#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>
#include <QVector>
#include <QPixmap>
#include <QStringList>

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QCloseEvent>
#include <QContextMenuEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum class PetState {
    SayHello,
    Swing,
    Sleep
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void loadAnimationFrames();
    void smoothSwingAnimation();
    void changeState(PetState newState);
    void setupTrayIcon();
    void launchAgent();
    void releaseSystemMemory();
    void showAboutDialog();

    Ui::MainWindow *ui;

    QPoint dragPosition;

    QTimer *timer;
    QTimer *stateTimer;  // For state transitions (sleep duration, etc)

    // Animation frames for each state
    QVector<QPixmap> sayHelloFrames;
    QVector<QPixmap> swingFrames;
    QVector<QPixmap> sleepFrames;

    QVector<QPixmap> *currentFrames;  // Points to current state's frames

    int frameIndex;
    PetState currentState;
    int stateElapsedMs;  // Time elapsed in current state (ms)

    // Tray icon and context menu
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QAction *actionLaunchAgent;
    QAction *actionFreeMemory;
    QAction *actionAbout;
    QAction *actionExit;
};

#endif
