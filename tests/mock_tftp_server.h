#ifndef MOCK_TFTP_SERVER_H
#define MOCK_TFTP_SERVER_H

#include <QByteArray>
#include <QHostAddress>
#include <QSemaphore>
#include <QThread>
#include <QUdpSocket>
#include <QtEndian>

// ---------------------------------------------------------------------------
// Lightweight mock TFTP server that runs in its own thread.
// Supports download (RRQ) and upload (WRQ) flows, plus error injection.
// ---------------------------------------------------------------------------
class MockTftpServer : public QThread {
    Q_OBJECT

public:
    enum Mode { ServeDownload, AcceptUpload, SendError };

    explicit MockTftpServer(QObject *parent = nullptr)
        : QThread(parent) {}

    ~MockTftpServer() override
    {
        requestInterruption();
        if (!wait(2000))
            terminate();
    }

    void configure(Mode mode, const QByteArray &content = {},
                   uint16_t errCode = 0, const QString &errMsg = {})
    {
        mode_    = mode;
        content_ = content;
        errCode_ = errCode;
        errMsg_  = errMsg;
    }

    quint16 serverPort() const     { return port_; }
    QByteArray receivedData() const { return received_; }

    void waitForReady() { ready_.acquire(); }

protected:
    void run() override
    {
        QUdpSocket sock;
        sock.bind(QHostAddress::LocalHost, 0);
        port_ = sock.localPort();
        ready_.release();

        for (int i = 0; i < 100 && !isInterruptionRequested(); ++i) {
            if (sock.waitForReadyRead(100))
                break;
        }
        if (isInterruptionRequested() || !sock.hasPendingDatagrams())
            return;

        QByteArray req;
        QHostAddress clientAddr;
        quint16 clientPort = 0;
        req.resize(static_cast<int>(sock.pendingDatagramSize()));
        sock.readDatagram(req.data(), req.size(), &clientAddr, &clientPort);

        if (req.size() < 4)
            return;

        uint16_t opcode = qFromBigEndian<uint16_t>(
            reinterpret_cast<const uchar *>(req.constData()));

        if (mode_ == SendError) {
            sendError(sock, clientAddr, clientPort);
            return;
        }

        if (opcode == 1 && mode_ == ServeDownload)
            handleRrq(sock, clientAddr, clientPort);
        else if (opcode == 2 && mode_ == AcceptUpload)
            handleWrq(sock, clientAddr, clientPort);
    }

private:
    // -- helpers ----------------------------------------------------------

    static QByteArray makeU16(uint16_t val)
    {
        uint16_t be = qToBigEndian(val);
        return QByteArray(reinterpret_cast<const char *>(&be), 2);
    }

    void sendError(QUdpSocket &sock,
                   const QHostAddress &addr, quint16 port)
    {
        QByteArray pkt;
        pkt += makeU16(5);
        pkt += makeU16(errCode_);
        pkt += errMsg_.toUtf8();
        pkt += '\0';
        sock.writeDatagram(pkt, addr, port);
    }

    // Server sends DATA blocks for the configured content.
    void handleRrq(QUdpSocket &sock,
                   const QHostAddress &addr, quint16 port)
    {
        int offset = 0;
        uint16_t blockNum = 1;

        while (offset <= content_.size()) {
            int len = qMin(content_.size() - offset, 512);
            QByteArray payload = content_.mid(offset, len);

            QByteArray pkt;
            pkt += makeU16(3);             // DATA opcode
            pkt += makeU16(blockNum);
            pkt += payload;
            sock.writeDatagram(pkt, addr, port);

            if (!sock.waitForReadyRead(5000))
                return;

            QByteArray ack;
            ack.resize(static_cast<int>(sock.pendingDatagramSize()));
            sock.readDatagram(ack.data(), ack.size());

            offset += len;
            ++blockNum;
            if (len < 512) break;
        }
    }

    // Server ACKs each DATA block the client sends.
    void handleWrq(QUdpSocket &sock,
                   const QHostAddress &addr, quint16 port)
    {
        // ACK 0 (accept WRQ)
        sock.writeDatagram(makeU16(4) + makeU16(0), addr, port);

        received_.clear();
        uint16_t expectedBlock = 1;

        for (;;) {
            if (!sock.waitForReadyRead(5000))
                return;

            QByteArray data;
            QHostAddress sAddr;
            quint16 sPort = 0;
            data.resize(static_cast<int>(sock.pendingDatagramSize()));
            sock.readDatagram(data.data(), data.size(), &sAddr, &sPort);

            if (data.size() < 4) continue;

            uint16_t op  = qFromBigEndian<uint16_t>(
                               reinterpret_cast<const uchar *>(data.constData()));
            uint16_t blk = qFromBigEndian<uint16_t>(
                               reinterpret_cast<const uchar *>(data.constData() + 2));

            if (op == 3 && blk == expectedBlock) {
                QByteArray payload = data.mid(4);
                received_.append(payload);

                sock.writeDatagram(makeU16(4) + makeU16(expectedBlock),
                                   sAddr, sPort);

                if (payload.size() < 512) break;
                ++expectedBlock;
            }
        }
    }

    Mode       mode_    = ServeDownload;
    QByteArray content_;
    QByteArray received_;
    quint16    port_    = 0;
    uint16_t   errCode_ = 0;
    QString    errMsg_;
    QSemaphore ready_;
};

#endif // MOCK_TFTP_SERVER_H
