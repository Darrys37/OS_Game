#include "gamesave.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

GameSave::GameSave(QObject *parent) : QObject(parent)
{
}

bool GameSave::saveGame(const GameState &gameState, QWidget *parent)
{
    // Ask user for save location
    QString filename = QFileDialog::getSaveFileName(
        parent,
        "Lưu Game",
        QDir::homePath() + "/ball_game_save.bgsave",
        getSaveFileFilter()
        );

    if (filename.isEmpty()) {
        return false; // User canceled
    }

    // Ensure file has proper extension
    if (!filename.endsWith(".bgsave", Qt::CaseInsensitive) &&
        !filename.endsWith(".json", Qt::CaseInsensitive)) {
        filename += ".bgsave";
    }

    // Create JSON object for game state
    QJsonObject gameStateObj;

    // Save balls array
    QJsonArray ballsArray;
    for (const BallData &ball : gameState.balls) {
        ballsArray.append(ballToJson(ball));
    }
    gameStateObj["balls"] = ballsArray;

    // Save game metadata
    gameStateObj["nextBallId"] = gameState.nextBallId;
    gameStateObj["selectedBallIndex"] = gameState.selectedBallIndex;
    gameStateObj["movingBallIndex"] = gameState.movingBallIndex;
    gameStateObj["score"] = gameState.score; // <<< THÊM DÒNG NÀY

    // Save timestamp and version for compatibility
    gameStateObj["saveVersion"] = "1.0";
    gameStateObj["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    gameStateObj["gameName"] = "Ball Game";

    // Create JSON document
    QJsonDocument doc(gameStateObj);

    // Write to file
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(parent, "Lỗi Lưu Game",
                             QString("Không thể tạo file:\n%1\n\nLỗi: %2")
                                 .arg(filename).arg(file.errorString()));
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // Show success message
    QMessageBox::information(parent, "Thành Công",
                             QString("Game đã được lưu thành công!\n\nFile: %1\nSố lượng bóng: %2")
                                 .arg(QFileInfo(filename).fileName())
                                 .arg(gameState.balls.size()));

    return true;
}

bool GameSave::loadGame(GameState &gameState, QWidget *parent)
{
    // Ask user for file to load
    QString filename = QFileDialog::getOpenFileName(
        parent,
        "Mở Game",
        QDir::homePath(),
        getLoadFileFilter()
        );

    if (filename.isEmpty()) {
        return false; // User canceled
    }

    // Read file
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(parent, "Lỗi Mở Game",
                             QString("Không thể mở file:\n%1\n\nLỗi: %2")
                                 .arg(filename).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(parent, "Lỗi File",
                             "File không hợp lệ hoặc đã bị hỏng!");
        return false;
    }

    QJsonObject gameStateObj = doc.object();

    // Validate basic structure
    if (!gameStateObj.contains("balls") || !gameStateObj["balls"].isArray()) {
        QMessageBox::warning(parent, "Lỗi File",
                             "File không chứa dữ liệu game hợp lệ!");
        return false;
    }

    // Clear current game state
    gameState.balls.clear();

    // Load balls
    QJsonArray ballsArray = gameStateObj["balls"].toArray();
    for (const QJsonValue &value : ballsArray) {
        if (value.isObject()) {
            BallData ball = jsonToBall(value.toObject());
            gameState.balls.append(ball);
        }
    }

    // Load metadata with fallback values
    gameState.nextBallId = gameStateObj.value("nextBallId").toInt(0);
    gameState.selectedBallIndex = gameStateObj.value("selectedBallIndex").toInt(-1);
    gameState.movingBallIndex = gameStateObj.value("movingBallIndex").toInt(-1);
    gameState.score = gameStateObj.value("score").toInt(0); // <<< THÊM DÒNG NÀY

    // Validate loaded data
    if (gameState.balls.isEmpty()) {
        QMessageBox::warning(parent, "Lỗi Dữ Liệu",
                             "File không chứa bóng nào!");
        return false;
    }

    // Update nextBallId if necessary (find maximum ID)
    int maxId = 0;
    for (const BallData &ball : gameState.balls) {
        if (ball.id > maxId) {
            maxId = ball.id;
        }
    }
    if (maxId >= gameState.nextBallId) {
        gameState.nextBallId = maxId + 1;
    }

    // Validate ball positions
    for (const BallData &ball : gameState.balls) {
        if (ball.row < 0 || ball.row >= 10 || ball.col < 0 || ball.col >= 10) {
            QMessageBox::warning(parent, "Lỗi Dữ Liệu",
                                 QString("Bóng có vị trí không hợp lệ:\nBóng ID %1 tại (%2,%3)")
                                     .arg(ball.id).arg(ball.row).arg(ball.col));  // ĐÃ SỬA: bỏ dấu ) thừa
            return false;
        }
    }

    // Check for duplicate positions
    QVector<QPoint> positions;
    for (const BallData &ball : gameState.balls) {
        QPoint pos(ball.row, ball.col);
        if (positions.contains(pos)) {
            QMessageBox::warning(parent, "Lỗi Dữ Liệu",
                                 QString("Nhiều bóng ở cùng vị trí:\nHàng %1, Cột %2")
                                     .arg(ball.row).arg(ball.col));  // ĐÃ SỬA: bỏ dấu ) thừa
            return false;
        }
        positions.append(pos);
    }

    // Show success message
    QMessageBox::information(parent, "Thành Công",
                             QString("Game đã được tải thành công!\n\nFile: %1\nSố lượng bóng: %2")
                                 .arg(QFileInfo(filename).fileName())
                                 .arg(gameState.balls.size()));

    return true;
}

QJsonObject GameSave::ballToJson(const BallData &ball)
{
    QJsonObject obj;
    obj["id"] = ball.id;
    obj["row"] = ball.row;
    obj["col"] = ball.col;
    obj["color"] = ball.color.name();
    obj["bounceOffset"] = ball.bounceOffset;
    return obj;
}

GameSave::BallData GameSave::jsonToBall(const QJsonObject &json)
{
    BallData ball;

    ball.id = json.value("id").toInt(-1);
    ball.row = json.value("row").toInt(0);
    ball.col = json.value("col").toInt(0);
    ball.bounceOffset = json.value("bounceOffset").toInt(0);

    // Handle color - support both name and RGB values
    if (json.contains("color")) {
        if (json["color"].isString()) {
            ball.color = QColor(json["color"].toString());
        } else if (json["color"].isObject()) {
            QJsonObject colorObj = json["color"].toObject();
            ball.color = QColor(
                colorObj.value("r").toInt(0),
                colorObj.value("g").toInt(0),
                colorObj.value("b").toInt(0)
                );
        }
    }

    // If color is invalid, use default red
    if (!ball.color.isValid()) {
        ball.color = QColor(255, 0, 0);
    }

    return ball;
}
