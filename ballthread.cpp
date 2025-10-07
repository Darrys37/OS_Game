#include "ballthread.h"
#include <QThread>
#include <QMutexLocker>

BallThread::BallThread(int ballId, QObject *parent)
    : QThread(parent),
    m_ballId(ballId),
    m_bouncing(false),
    m_abort(false),
    m_offset(0),
    m_dir(1)
{
}

BallThread::~BallThread()
{
    stopAndWait();
}

void BallThread::startBouncing()
{
    if (!isRunning()) {
        m_abort.store(false);
        start();
    }
    m_bouncing.store(true);
}

void BallThread::stopBouncing()
{
    m_bouncing.store(false);
}

void BallThread::stopAndWait()
{
    m_abort.store(true);
    m_bouncing.store(false);
    if (isRunning()) {
        wait(500);
    }
}

void BallThread::run()
{
    m_offset = 0;
    m_dir = 1;

    while (!m_abort.load()) {
        if (m_bouncing.load()) {
            m_offset += m_dir;
            if (m_offset > 5 || m_offset < -5)
                m_dir *= -1;

            emit bounceUpdated(m_ballId, m_offset);
        }
        msleep(30);
    }
}
