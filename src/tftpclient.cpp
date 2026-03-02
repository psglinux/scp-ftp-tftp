#include "tftpclient.h"

#include <QFile>
#include <QHostAddress>
#include <QUdpSocket>
#include <QtEndian>

TftpClient::TftpClient(QObject *parent)
    : QObject(parent) {}

// ---------------------------------------------------------------------------
// Packet builders  (RFC 1350)
// ---------------------------------------------------------------------------
QByteArray TftpClient::requestPacket(Opcode op, const QString &filename)
{
    QByteArray pkt;
    uint16_t opBE = qToBigEndian(static_cast<uint16_t>(op));
    pkt.append(reinterpret_cast<const char *>(&opBE), 2);
    pkt.append(filename.toUtf8());
    pkt.append('\0');
    pkt.append("octet");
    pkt.append('\0');
    return pkt;
}

QByteArray TftpClient::ackPacket(uint16_t block)
{
    QByteArray pkt;
    uint16_t opBE  = qToBigEndian(static_cast<uint16_t>(ACK));
    uint16_t blkBE = qToBigEndian(block);
    pkt.append(reinterpret_cast<const char *>(&opBE),  2);
    pkt.append(reinterpret_cast<const char *>(&blkBE), 2);
    return pkt;
}

QByteArray TftpClient::dataPacket(uint16_t block, const QByteArray &payload)
{
    QByteArray pkt;
    uint16_t opBE  = qToBigEndian(static_cast<uint16_t>(DATA));
    uint16_t blkBE = qToBigEndian(block);
    pkt.append(reinterpret_cast<const char *>(&opBE),  2);
    pkt.append(reinterpret_cast<const char *>(&blkBE), 2);
    pkt.append(payload);
    return pkt;
}

// ---------------------------------------------------------------------------
// Download (RRQ)
// ---------------------------------------------------------------------------
bool TftpClient::download(const QString &host, int port,
                           const QString &remotePath, const QString &localPath,
                           std::atomic<bool> *cancelled)
{
    QUdpSocket sock;
    QHostAddress addr(host);

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit logMessage("Cannot open local file for writing: " + localPath);
        return false;
    }

    sock.writeDatagram(requestPacket(RRQ, remotePath),
                       addr, static_cast<quint16>(port));
    emit logMessage("Sent RRQ for " + remotePath);

    qint64   totalReceived = 0;
    uint16_t expectedBlock = 1;

    for (;;) {
        if (cancelled && cancelled->load()) {
            emit logMessage("Transfer cancelled.");
            file.close();
            QFile::remove(localPath);
            return false;
        }

        if (!sock.waitForReadyRead(TIMEOUT_MS)) {
            emit logMessage("Timeout waiting for DATA packet.");
            file.close();
            return false;
        }

        QByteArray dgram;
        QHostAddress sender;
        quint16 senderPort = 0;
        dgram.resize(static_cast<int>(sock.pendingDatagramSize()));
        sock.readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);

        if (dgram.size() < 4) continue;

        auto opcode = qFromBigEndian<uint16_t>(
                          reinterpret_cast<const uchar *>(dgram.constData()));

        if (opcode == ERR) {
            emit logMessage("TFTP error from server: "
                            + QString::fromUtf8(dgram.mid(4)));
            file.close();
            return false;
        }
        if (opcode != DATA) continue;

        auto blockNum = qFromBigEndian<uint16_t>(
                            reinterpret_cast<const uchar *>(dgram.constData() + 2));

        if (blockNum != expectedBlock) continue;

        QByteArray payload = dgram.mid(4);
        file.write(payload);
        totalReceived += payload.size();

        sock.writeDatagram(ackPacket(blockNum), sender, senderPort);
        emit progressChanged(totalReceived, 0);

        if (payload.size() < BLOCK_SIZE) {
            emit logMessage(QStringLiteral("Download complete — %1 bytes received.")
                                .arg(totalReceived));
            file.close();
            return true;
        }
        ++expectedBlock;
    }
}

// ---------------------------------------------------------------------------
// Upload (WRQ)
// ---------------------------------------------------------------------------
bool TftpClient::upload(const QString &host, int port,
                         const QString &localPath, const QString &remotePath,
                         std::atomic<bool> *cancelled)
{
    QUdpSocket sock;
    QHostAddress addr(host);

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage("Cannot open local file for reading: " + localPath);
        return false;
    }
    const qint64 fileSize = file.size();

    sock.writeDatagram(requestPacket(WRQ, remotePath),
                       addr, static_cast<quint16>(port));
    emit logMessage("Sent WRQ for " + remotePath);

    if (!sock.waitForReadyRead(TIMEOUT_MS)) {
        emit logMessage("Timeout waiting for WRQ acknowledgement.");
        return false;
    }

    QByteArray dgram;
    QHostAddress sender;
    quint16 senderPort = 0;
    dgram.resize(static_cast<int>(sock.pendingDatagramSize()));
    sock.readDatagram(dgram.data(), dgram.size(), &sender, &senderPort);

    if (dgram.size() >= 4) {
        auto op = qFromBigEndian<uint16_t>(
                      reinterpret_cast<const uchar *>(dgram.constData()));
        if (op == ERR) {
            emit logMessage("TFTP error: " + QString::fromUtf8(dgram.mid(4)));
            return false;
        }
    }

    uint16_t blockNum = 1;
    qint64   totalSent = 0;

    for (;;) {
        if (cancelled && cancelled->load()) {
            emit logMessage("Transfer cancelled.");
            return false;
        }

        QByteArray payload = file.read(BLOCK_SIZE);
        QByteArray pkt     = dataPacket(blockNum, payload);

        bool acked = false;
        for (int retry = 0; retry < MAX_RETRIES && !acked; ++retry) {
            sock.writeDatagram(pkt, sender, senderPort);

            if (!sock.waitForReadyRead(TIMEOUT_MS))
                continue;

            QByteArray resp;
            resp.resize(static_cast<int>(sock.pendingDatagramSize()));
            sock.readDatagram(resp.data(), resp.size());

            if (resp.size() < 4) continue;

            auto op  = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(resp.constData()));
            auto blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(resp.constData() + 2));

            if (op == ACK && blk == blockNum) {
                acked = true;
            } else if (op == ERR) {
                emit logMessage("TFTP error: " + QString::fromUtf8(resp.mid(4)));
                return false;
            }
        }

        if (!acked) {
            emit logMessage("Max retries exceeded — transfer failed.");
            return false;
        }

        totalSent += payload.size();
        emit progressChanged(totalSent, fileSize);

        if (payload.size() < BLOCK_SIZE) {
            emit logMessage(QStringLiteral("Upload complete — %1 bytes sent.")
                                .arg(totalSent));
            return true;
        }
        ++blockNum;
    }
}
