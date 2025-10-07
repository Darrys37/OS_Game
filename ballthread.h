#ifndef BALLTHREAD_H
#define BALLTHREAD_H

#include <QThread>
#include <QObject>
#include <QMutex>
#include <atomic>

class BallThread : public QThread
{
    Q_OBJECT

public:
    explicit BallThread(int ballId, QObject *parent = nullptr);
    ~BallThread();

    void startBouncing();       // bật hiệu ứng nảy
    void stopBouncing();        // tắt hiệu ứng nảy (thread vẫn sống)
    void stopAndWait();         // dừng hẳn thread (dùng khi cleanup)

    bool isBouncing() const { return m_bouncing.load(); }

signals:
    void bounceUpdated(int ballId, int bounceOffset);

protected:
    void run() override;

private:
    int m_ballId;
    std::atomic<bool> m_bouncing;   // đang nảy hay không
    std::atomic<bool> m_abort;      // yêu cầu thoát thread
    int m_offset;
    int m_dir;
    QMutex m_mutex;
};

#endif // BALLTHREAD_H
