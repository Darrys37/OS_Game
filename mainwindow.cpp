#include "mainwindow.h"
#include <QHeaderView>
#include <QFrame>
#include <QRandomGenerator>
#include <QTime>
#include <QApplication>
#include <QPainter>
#include <QPixmap>

#include <queue>
#include <map>

// Kiểm tra cell có bị chiếm (bởi 1 quả khác) - ngoại trừ excludeBallIndex
bool MainWindow::isCellOccupiedExcept(int row, int col, int excludeBallIndex)
{
    for (int i = 0; i < balls.size(); ++i) {
        if (i == excludeBallIndex) continue;
        const Ball &b = balls.at(i);
        if (b.row == row && b.col == col) return true;
    }
    return false;
}

// A* to find path from (sr,sc) to (tr,tc). Returns empty vector if no path.
// Grid 10x10 assumed.
QVector<QPoint> MainWindow::findPath(int sr, int sc, int tr, int tc)
{
    const int R = table->rowCount();
    const int C = table->columnCount();
    if (sr == tr && sc == tc) return QVector<QPoint>{QPoint(sr, sc)};

    auto inBounds = [&](int r, int c){ return r >= 0 && r < R && c >= 0 && c < C; };

    struct Node {
        int r, c;
        int f, g;
        bool operator<(Node const& other) const { return f > other.f; } // for min-heap
    };

    QVector<QVector<int>> gscore(R, QVector<int>(C, INT_MAX));
    QVector<QVector<QPoint>> parent(R, QVector<QPoint>(C, QPoint(-1,-1)));
    QVector<QVector<bool>> closed(R, QVector<bool>(C,false));

    auto heuristic = [&](int r,int c){ return abs(r - tr) + abs(c - tc); };

    std::priority_queue<Node> open;
    gscore[sr][sc] = 0;
    open.push(Node{sr, sc, heuristic(sr,sc), 0});

    // allow start cell even if occupied (by the selected ball)
    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        int r = cur.r, c = cur.c;
        if (closed[r][c]) continue;
        closed[r][c] = true;
        if (r == tr && c == tc) break;

        const int dr[4] = {-1,1,0,0};
        const int dc[4] = {0,0,-1,1};
        for (int k=0;k<4;++k) {
            int nr = r + dr[k];
            int nc = c + dc[k];
            if (!inBounds(nr,nc)) continue;
            // treat target cell as walkable (if it is empty), treat other cells walkable only if not occupied
            if (nr == tr && nc == tc) {
                // ok even if target equals some condition — still should be free
                if (isCellOccupiedExcept(nr, nc, movingBallIndex)) continue;
            } else {
                if (isCellOccupiedExcept(nr, nc, movingBallIndex)) continue;
            }

            int tentative_g = gscore[r][c] + 1;
            if (tentative_g < gscore[nr][nc]) {
                gscore[nr][nc] = tentative_g;
                parent[nr][nc] = QPoint(r,c);
                int f = tentative_g + heuristic(nr,nc);
                open.push(Node{nr,nc,f,tentative_g});
            }
        }
    }

    // reconstruct
    if (gscore[tr][tc] == INT_MAX) return QVector<QPoint>(); // no path

    QVector<QPoint> path;
    QPoint cur(tr,tc);
    while (!(cur.x() == sr && cur.y() == sc)) {
        path.prepend(cur);
        QPoint p = parent[cur.x()][cur.y()];
        if (p.x() == -1) break;
        cur = p;
    }
    path.prepend(QPoint(sr,sc));
    return path;
}
void MainWindow::startMoveAlongPath(const QVector<QPoint> &path, int ballIndex)
{
    if (path.isEmpty() || ballIndex < 0 || ballIndex >= balls.size()) return;

    // stop any existing movement
    if (moveTimer && moveTimer->isActive()) stopMovement();

    currentPath = path;
    currentPathStep = 1;
    movingBallIndex = ballIndex;

    // stop bouncing for moving ball
    if (balls[ballIndex].thread) balls[ballIndex].thread->stopBouncing();

    if (!moveTimer) {
        moveTimer = new QTimer(this);
        connect(moveTimer, &QTimer::timeout, this, [this]() {
            if (currentPathStep >= currentPath.size()) {
                stopMovement();
                return;
            }
            QPoint p = currentPath[currentPathStep];
            if (movingBallIndex >= 0 && movingBallIndex < balls.size()) {
                balls[movingBallIndex].row = p.x();
                balls[movingBallIndex].col = p.y();
            }
            currentPathStep++;
            updateBallPositions();
        });
    }

    moveTimer->start(150);
    // keep selectedBallIndex = ballIndex while moving (optional)
    selectedBallIndex = ballIndex;
    updateBallPositions();
}

void MainWindow::stopMovement()
{
    if (moveTimer && moveTimer->isActive()) {
        moveTimer->stop();
    }
    currentPath.clear();
    currentPathStep = 0;

    // Khi di chuyển xong: chỉ cho 1 quả nảy — quả đang được chọn
    if (movingBallIndex >= 0 && movingBallIndex < balls.size()) {
        for (int i = 0; i < balls.size(); ++i) {
            if (i == movingBallIndex) {
                if (balls[i].thread) balls[i].thread->startBouncing();
                selectedBallIndex = i; // chọn lại quả đang di chuyển
            } else {
                if (balls[i].thread) balls[i].thread->stopBouncing();
            }
        }
    }

    movingBallIndex = -1;
    updateBallPositions();
}





// Sửa constructor

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    nextBallId = 0;  // Khởi tạo trong thân hàm
    setupUi();
    setWindowTitle("Ball Game - 10x10 Grid");
    resize(1000, 800);
    // setMinimumSize(900, 900);


    initializeBalls();
}

MainWindow::~MainWindow()
{
    stopAllThreads();
}

void MainWindow::setupUi()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    createMenu();
    createContent();

    mainLayout->addWidget(leftMenu, 1);
    mainLayout->addWidget(rightContent, 4);
}

void MainWindow::createMenu()
{
    leftMenu = new QWidget(this);
    leftMenu->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2c3e50, stop:1 #34495e);"
        "color: white;"
        "border-right: 2px solid #1a252f;"
        );

    auto *menuLayout = new QVBoxLayout(leftMenu);
    menuLayout->setAlignment(Qt::AlignTop);
    menuLayout->setSpacing(15);
    menuLayout->setContentsMargins(20, 30, 20, 30);

    // Title
    titleLabel = new QLabel("BALL GAME", leftMenu);
    titleLabel->setStyleSheet(
        "font-size: 24px;"
        "font-weight: bold;"
        "color: #ecf0f1;"
        "padding: 10px;"
        "background: rgba(255,255,255,0.1);"
        "border-radius: 10px;"
        "text-align: center;"
        );
    titleLabel->setAlignment(Qt::AlignCenter);

    // Randomize button
    randomizeButton = new QPushButton("🎲 Random Balls", leftMenu);
    randomizeButton->setStyleSheet(
        "QPushButton {"
        "    background: #9b59b6;"
        "    color: white;"
        "    border: none;"
        "    padding: 12px;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: #8e44ad;"
        "}"
        "QPushButton:pressed {"
        "    background: #7d3c98;"
        "}"
        );
    randomizeButton->setMinimumHeight(45);

    // Restart button
    restartButton = new QPushButton("🔄 Vị trí ban đầu", leftMenu);
    restartButton->setStyleSheet(
        "QPushButton {"
        "    background: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 12px;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "    background: #21618c;"
        "}"
        );
    restartButton->setMinimumHeight(45);

    // Close button
    closeButton = new QPushButton("✕ Tắt chương trình", leftMenu);
    closeButton->setStyleSheet(
        "QPushButton {"
        "    background: #e74c3c;"
        "    color: white;"
        "    border: none;"
        "    padding: 12px;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: #c0392b;"
        "}"
        "QPushButton:pressed {"
        "    background: #a93226;"
        "}"
        );
    closeButton->setMinimumHeight(45);

    menuLayout->addWidget(titleLabel);
    menuLayout->addSpacing(30);
    menuLayout->addWidget(randomizeButton);
    menuLayout->addWidget(restartButton);
    menuLayout->addStretch(1);
    menuLayout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, &MainWindow::onCloseClicked);
    connect(restartButton, &QPushButton::clicked, this, &MainWindow::onRestartClicked);
    connect(randomizeButton, &QPushButton::clicked, this, &MainWindow::onRandomizeClicked);
}

void MainWindow::createContent()
{
    rightContent = new QWidget(this);
    rightContent->setStyleSheet("background: #ecf0f1;");

    auto *contentLayout = new QVBoxLayout(rightContent);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    contentLayout->setSpacing(20);

    // Header
    auto *headerLabel = new QLabel("BẢNG 10x10 VỚI BÓNG NẢY (THREAD)", rightContent);
    headerLabel->setStyleSheet(
        "font-size: 28px;"
        "font-weight: bold;"
        "color: #2c3e50;"
        "padding: 15px;"
        "background: white;"
        "border-radius: 15px;"
        "text-align: center;"
        );
    headerLabel->setAlignment(Qt::AlignCenter);

    // Info label
    auto *infoLabel = new QLabel("Click vào trái banh để làm nó nảy lên xuống (mỗi bóng chạy trên thread riêng)!", rightContent);
    infoLabel->setStyleSheet(
        "font-size: 14px;"
        "color: #7f8c8d;"
        "padding: 10px;"
        "background: #d5dbdb;"
        "border-radius: 8px;"
        "text-align: center;"
        );
    infoLabel->setAlignment(Qt::AlignCenter);

    // Create table
    table = new QTableWidget(10, 10, rightContent);
    table->setStyleSheet(
        "QTableWidget {"
        "    background: white;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 10px;"
        "    gridline-color: #ecf0f1;"
        "}"
        "QTableWidget::item {"
        "    border: 1px solid #ecf0f1;"
        "    padding: 5px;"
        "}"
        "QTableWidget::item:selected {"
        "    background: #3498db;"
        "}"
        "QHeaderView::section {"
        "    background: #34495e;"
        "    color: white;"
        "    padding: 8px;"
        "    border: 1px solid #2c3e50;"
        "    font-weight: bold;"
        "}"
        );

    // Configure table
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);

    // Set row and column sizes
    int cellSize = 55;
    for (int i = 0; i < 10; ++i) {
        table->setRowHeight(i, cellSize);
        table->setColumnWidth(i, cellSize);
    }

    // Ẩn thanh cuộn
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set headers
    QStringList headers;
    for (int i = 1; i <= 10; ++i) {
        headers << QString::number(i);
    }
    table->setHorizontalHeaderLabels(headers);
    table->setVerticalHeaderLabels(headers);

    // Clear initial cells
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 10; ++col) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            item->setBackground(QBrush(QColor(240, 240, 240)));
            table->setItem(row, col, item);
        }
    }

    connect(table, &QTableWidget::cellClicked, this, &MainWindow::onCellClicked);

    contentLayout->addWidget(headerLabel);
    contentLayout->addWidget(infoLabel);
    contentLayout->addWidget(table, 1);
}

int MainWindow::getRandomInt(int min, int max)
{
    return QRandomGenerator::global()->bounded(min, max + 1);
}

bool MainWindow::isBallAt(int row, int col)
{
    for (const Ball &ball : balls) {
        if (ball.row == row && ball.col == col) {
            return true;
        }
    }
    return false;
}

// -------------------------
// An toàn dừng và xóa thread
// -------------------------
void MainWindow::stopAllThreads()
{
    for (Ball &ball : balls) {
        if (ball.thread) {
            ball.thread->stopAndWait();  // ✅ thay vì stopBouncing + wait
            disconnect(ball.thread, nullptr, this, nullptr);
            delete ball.thread;
            ball.thread = nullptr;
        }
    }
}



void MainWindow::initializeBalls()
{
    stopAllThreads();
    balls.clear();

    // Vị trí cố định ban đầu cho 3 trái banh
    QVector<QPoint> initialPositions = {
        QPoint(2, 2),  // Ball 1
        QPoint(5, 5),  // Ball 2
        QPoint(8, 8)   // Ball 3
    };

    QVector<QColor> initialColors = {
        QColor(255, 0, 0),   // Red
        QColor(0, 255, 0),   // Green
        QColor(0, 0, 255)    // Blue
    };

    for (int i = 0; i < 3; ++i) {
        Ball ball;
        ball.id = nextBallId++;
        ball.row = initialPositions[i].x();
        ball.col = initialPositions[i].y();
        ball.color = initialColors[i];
        ball.bounceOffset = 0;
        ball.thread = new BallThread(ball.id, this);

        connect(ball.thread, &BallThread::bounceUpdated,
                this, &MainWindow::onBounceUpdated);

        balls.append(ball);
    }

    updateBallPositions();
}



QColor MainWindow::getRandomColor()
{
    static const QVector<QColor> colors = {
        QColor(255, 0, 0),     // Red
        QColor(0, 255, 0),     // Green
        QColor(0, 0, 255),     // Blue
        QColor(255, 255, 0),   // Yellow
        QColor(255, 0, 255),   // Magenta
        QColor(0, 255, 255),   // Cyan
        QColor(255, 165, 0),   // Orange
        QColor(128, 0, 128),   // Purple
        QColor(255, 192, 203), // Pink
        QColor(0, 128, 0),     // Dark Green
        QColor(139, 69, 19),   // Brown
        QColor(0, 0, 128)      // Navy
    };

    return colors[getRandomInt(0, colors.size() - 1)];
}

// -------------------------
// Cập nhật hiển thị bóng (vẽ circle bằng QPixmap + QLabel trong cell)
// -------------------------
void MainWindow::updateBallPositions()
{
    // Clear old widgets safely
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QWidget *old = table->cellWidget(row, col);
            if (old) {
                table->removeCellWidget(row, col);
                delete old;
            }
            QTableWidgetItem *item = table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                table->setItem(row, col, item);
            }
            item->setText("");
            item->setBackground(QBrush(QColor(240, 240, 240)));
            item->setToolTip(QString()); // clear tooltip
        }
    }

    // Draw balls
    for (const Ball &ball : balls) {
        if (ball.row < 0 || ball.row >= table->rowCount() ||
            ball.col < 0 || ball.col >= table->columnCount()) continue;

        int cellH = table->rowHeight(ball.row);
        int cellW = table->columnWidth(ball.col);
        int diameter = qMax(8, static_cast<int>(qMin(cellH, cellW) * 0.6)); // 60% ô

        QPixmap pixmap(diameter, diameter);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(ball.color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(0, 0, diameter - 1, diameter - 1);
        painter.end();

        QLabel *ballLabel = new QLabel();
        ballLabel->setAttribute(Qt::WA_TranslucentBackground);
        ballLabel->setStyleSheet("background: transparent;");
        ballLabel->setPixmap(pixmap);
        ballLabel->setFixedSize(diameter, diameter);
        ballLabel->setScaledContents(false);
        ballLabel->setAlignment(Qt::AlignCenter);

        // Make both container and label ignore mouse events so table receives clicks
        QWidget *container = new QWidget();
        container->setAttribute(Qt::WA_TranslucentBackground);
        container->setStyleSheet("background: transparent;");

        // IMPORTANT: ignore mouse events so clicks fall through to the table (cellClicked will fire)
        container->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        ballLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        QVBoxLayout *lay = new QVBoxLayout(container);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);

        int bounce = qBound(-5, ball.bounceOffset, 5);
        int topSpacer = 5 - bounce;
        int bottomSpacer = 5 + bounce;
        lay->addSpacerItem(new QSpacerItem(10, topSpacer, QSizePolicy::Minimum, QSizePolicy::Fixed));
        lay->addWidget(ballLabel, 0, Qt::AlignHCenter);
        lay->addSpacerItem(new QSpacerItem(10, bottomSpacer, QSizePolicy::Minimum, QSizePolicy::Fixed));

        table->setCellWidget(ball.row, ball.col, container);

        // Put tooltip on the item (not on container) so it still shows.
        QTableWidgetItem *item = table->item(ball.row, ball.col);
        if (item) {
            item->setToolTip(QString("Ball %1\nPos: (%2,%3)").arg(ball.id).arg(ball.row+1).arg(ball.col+1));
        }
    }

    // Optionally, highlight selected cell (visual cue)
    // First clear selection background on all items (we already set background). Then mark selectedBallIndex
    if (selectedBallIndex >= 0 && selectedBallIndex < balls.size()) {
        const Ball &sb = balls[selectedBallIndex];
        QTableWidgetItem *it = table->item(sb.row, sb.col);
        if (it) {
            it->setBackground(QBrush(QColor(220, 240, 255))); // nhẹ highlight
        }
    }
}


// -------------------------
// onCellClicked an toàn (kiểm tra tồn tại ball + không truy cập thread null)
// -------------------------



void MainWindow::onRandomizeClicked()
{
    // stop any movement / selection
    stopMovement();
    stopAllThreads();

    // randomize positions/colors
    for (Ball &ball : balls) {
        bool positionExists;
        do {
            positionExists = false;
            ball.row = getRandomInt(0, 9);
            ball.col = getRandomInt(0, 9);
            for (const Ball &other : balls) {
                if (&ball != &other && ball.row == other.row && ball.col == other.col) {
                    positionExists = true; break;
                }
            }
        } while (positionExists);

        ball.color = getRandomColor();
        ball.bounceOffset = 0;

        // make sure thread exists but not started by default
        if (!ball.thread) {
            ball.thread = new BallThread(ball.id, this);
            connect(ball.thread, &BallThread::bounceUpdated, this, &MainWindow::onBounceUpdated);
        } else {
            ball.thread->stopBouncing();
        }
    }

    selectedBallIndex = -1;
    movingBallIndex = -1;
    updateBallPositions();
}




void MainWindow::onCellClicked(int row, int column)
{
    qDebug() << "Cell clicked:" << row << column;

    // If currently moving a ball, ignore clicks (avoid conflicts).
    if (movingBallIndex != -1) {
        qDebug() << "Ignored click while a ball is moving";
        return;
    }

    // Find clicked ball index
    int clickedIndex = -1;
    for (int i = 0; i < balls.size(); ++i) {
        if (balls[i].row == row && balls[i].col == column) {
            clickedIndex = i;
            break;
        }
    }

    // Clicked on a ball -> toggle selection
    if (clickedIndex != -1) {
        // if selecting a different ball, stop old one's bouncing
        if (selectedBallIndex != -1 && selectedBallIndex != clickedIndex) {
            Ball &old = balls[selectedBallIndex];
            if (old.thread) old.thread->stopBouncing();
        }

        Ball &b = balls[clickedIndex];
        if (selectedBallIndex == clickedIndex) {
            // toggle off
            if (b.thread) b.thread->stopBouncing();
            selectedBallIndex = -1;
        } else {
            // select this ball
            selectedBallIndex = clickedIndex;
            if (b.thread) b.thread->startBouncing();
        }

        updateBallPositions();
        return;
    }

    // Clicked empty cell
    if (selectedBallIndex == -1) {
        qDebug() << "Clicked empty cell with no selection - ignore";
        return;
    }

    // compute path and start moving
    Ball &sel = balls[selectedBallIndex];

    // Let pathfinder treat the selected ball as the moving one (exclude from occupancy)
    movingBallIndex = selectedBallIndex;
    QVector<QPoint> path = findPath(sel.row, sel.col, row, column);

    if (path.isEmpty()) {
        qDebug() << "No path found";
        movingBallIndex = -1;
        return;
    }

    // start movement (this will stop bouncing of that ball)
    startMoveAlongPath(path, selectedBallIndex);
}




void MainWindow::onBounceUpdated(int ballId, int bounceOffset)
{
    for (Ball &ball : balls) {
        if (ball.id == ballId) {
            ball.bounceOffset = bounceOffset;
            break;
        }
    }
    updateBallPositions();
}

void MainWindow::onCloseClicked()
{
    stopAllThreads();
    QApplication::quit();
}

void MainWindow::onRestartClicked()
{
    initializeBalls();
}
BallWorker::BallWorker() : running(false), direction(1) {}

void BallWorker::process() {
    running = true;
    int offset = 0;
    while (running) {
        offset += direction;
        if (offset > 5 || offset < -5) {
            direction *= -1;
        }
        emit positionChanged(0, offset); // Giả sử cập nhật cho ball 0
        QThread::msleep(30);
    }
}

void BallWorker::stop() {
    running = false;
}

// ---- MainWindow ----
void MainWindow::onStartAnimationClicked() {
    // Hiện tại chưa dùng, bạn có thể bỏ trống hoặc thêm logic start chung
    qDebug("Start animation clicked");
}

void MainWindow::onStopAnimationClicked() {
    // Tương tự
    qDebug("Stop animation clicked");
}

void MainWindow::onBallPositionChanged(int ballIndex, int offsetY) {
    // Tạm thời không cần làm gì nếu bạn không dùng BallWorker
    Q_UNUSED(ballIndex);
    Q_UNUSED(offsetY);
}

// ---- Event filter ----
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // Nếu bạn chưa xử lý event đặc biệt nào, chỉ cần:
    return QMainWindow::eventFilter(obj, event);
}
