#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QTimer>
#include <QColor>
#include <QThread>
#include "ballthread.h"  // Thêm include này
#include "gamesave.h"
class BallWorker : public QObject {
    Q_OBJECT
public:
    BallWorker();
    void stop();

signals:
    void positionChanged(int ballIndex, int offsetY);

public slots:
    void process();

private:
    bool running;
    int direction;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCloseClicked();
    void onRestartClicked();
    void onRandomizeClicked();
    void onStartAnimationClicked();
    void onStopAnimationClicked();
    void onBallPositionChanged(int ballIndex, int offsetY);
    void onCellClicked(int row, int column);  // Thêm slot này
    void onBounceUpdated(int ballId, int bounceOffset);  // Thêm slot này
    void addRandomBalls(int count);
    void checkAndRemoveLines();
    void onSaveGameClicked();
    void onLoadGameClicked();
private:
    void setupUi();
    void createMenu();
    void createContent();
    void initializeBalls();
    void updateBallPositions();
    void startBallAnimation();
    void stopBallAnimation();
    void stopAllThreads();  // Thêm hàm này
    bool isBallAt(int row, int col);  // Thêm hàm này
    QColor getRandomColor();
    int getRandomInt(int min, int max);
    bool eventFilter(QObject *obj, QEvent *event) override;
    QVector<QColor> baseColors; // <<< THÊM DÒNG NÀY

    // UI Components
    QWidget *centralWidget;
    QWidget *leftMenu;
    QWidget *rightContent;
    QTableWidget *table;
    QPushButton *closeButton;
    QPushButton *restartButton;
    QPushButton *randomizeButton;
    QPushButton *startAnimationButton;
    QPushButton *stopAnimationButton;
    QLabel *titleLabel;
    GameSave *gameSave;
    QPushButton *saveGameButton;   // Thêm dòng này
    QPushButton *loadGameButton;   // Thêm dòng này
    // Ball data
    struct Ball {
        int id;  // Thêm id
        int row;
        int col;
        QColor color;
        int bounceOffset;  // Đổi từ currentOffsetY
        BallThread *thread;  // Thêm thread
        QLabel *ballLabel;
    };

    QVector<Ball> balls;
    int nextBallId;  // Thêm biến này

    // Animation
    QThread *animationThread;
    BallWorker *ballWorker;
    bool isAnimating;
    // trong class MainWindow (private phần)
    int selectedBallIndex = -1;            // index trong QVector<Ball>, -1 nếu không chọn
    QTimer *moveTimer = nullptr;
    QVector<QPoint> currentPath;           // đường đi (list ô) cho movement
    int currentPathStep = 0;               // bước hiện tại trên path
    int movingBallIndex = -1;              // ball đang di chuyển, -1 nếu không
    void startMoveAlongPath(const QVector<QPoint> &path, int ballIndex);
    void stopMovement();
    QVector<QPoint> findPath(int sr, int sc, int tr, int tc);
    bool isCellOccupiedExcept(int row, int col, int excludeBallIndex);

};

#endif // MAINWINDOW_H
