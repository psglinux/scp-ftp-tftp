#ifndef TFTPCLIENT_H
#define TFTPCLIENT_H

#include <QObject>
#include <atomic>

class TftpClient : public QObject {
    Q_OBJECT

public:
    explicit TftpClient(QObject *parent = nullptr);

    static constexpr int BLOCK_SIZE   = 512;
    static constexpr int TIMEOUT_MS   = 5000;
    static constexpr int MAX_RETRIES  = 5;

    enum Opcode : uint16_t {
        RRQ   = 1,
        WRQ   = 2,
        DATA  = 3,
        ACK   = 4,
        ERR   = 5
    };

    bool upload(const QString &host, int port,
                const QString &localPath, const QString &remotePath,
                std::atomic<bool> *cancelled);

    bool download(const QString &host, int port,
                  const QString &remotePath, const QString &localPath,
                  std::atomic<bool> *cancelled);

    static QByteArray requestPacket(Opcode op, const QString &filename);
    static QByteArray ackPacket(uint16_t block);
    static QByteArray dataPacket(uint16_t block, const QByteArray &payload);

signals:
    void progressChanged(qint64 transferred, qint64 total);
    void logMessage(const QString &message);
};

#endif // TFTPCLIENT_H
