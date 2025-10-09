#ifndef GAMESAVE_H
#define GAMESAVE_H

#include <QObject>
#include <QVector>
#include <QColor>
#include <QPoint>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

class GameSave : public QObject
{
    Q_OBJECT

public:
    explicit GameSave(QObject *parent = nullptr);

    // Ball data structure for serialization
    struct BallData {
        int id;
        int row;
        int col;
        QColor color;
        int bounceOffset;

        BallData() : id(-1), row(0), col(0), bounceOffset(0) {}
        BallData(int id, int row, int col, const QColor &color, int bounceOffset)
            : id(id), row(row), col(col), color(color), bounceOffset(bounceOffset) {}
    };

    // Game state structure
    struct GameState {
        QVector<BallData> balls;
        int nextBallId;
        int selectedBallIndex;
        int movingBallIndex;
        int score; // <<< THÊM DÒNG NÀY
        GameState() : nextBallId(0), selectedBallIndex(-1), movingBallIndex(-1) {}
    };

    // Save game to file
    bool saveGame(const GameState &gameState, QWidget *parent = nullptr);

    // Load game from file
    bool loadGame(GameState &gameState, QWidget *parent = nullptr);

    // Convert between BallData and JSON
    static QJsonObject ballToJson(const BallData &ball);
    static BallData jsonToBall(const QJsonObject &json);

private:
    QString getSaveFileFilter() const { return "Ball Game Save Files (*.bgsave);;JSON Files (*.json);;All Files (*)"; }
    QString getLoadFileFilter() const { return "Ball Game Save Files (*.bgsave *.json);;All Files (*)"; }
};

#endif // GAMESAVE_H
