#include <QtTest>
#include <QtEndian>
#include "tftpclient.h"

class TestTftpPackets : public QObject {
    Q_OBJECT

private slots:
    // ---- RRQ / WRQ request packets ----

    void rrqPacketLayout()
    {
        QByteArray pkt = TftpClient::requestPacket(TftpClient::RRQ,
                                                    "firmware.bin");
        QVERIFY(pkt.size() >= 2 + 12 + 1 + 5 + 1);

        uint16_t op = qFromBigEndian<uint16_t>(
                          reinterpret_cast<const uchar *>(pkt.constData()));
        QCOMPARE(op, uint16_t(1));

        QByteArray name = pkt.mid(2, pkt.indexOf('\0', 2) - 2);
        QCOMPARE(name, QByteArray("firmware.bin"));

        int modeStart = 2 + name.size() + 1;
        QByteArray mode = pkt.mid(modeStart,
                                   pkt.indexOf('\0', modeStart) - modeStart);
        QCOMPARE(mode, QByteArray("octet"));
    }

    void wrqPacketLayout()
    {
        QByteArray pkt = TftpClient::requestPacket(TftpClient::WRQ,
                                                    "upload.dat");
        uint16_t op = qFromBigEndian<uint16_t>(
                          reinterpret_cast<const uchar *>(pkt.constData()));
        QCOMPARE(op, uint16_t(2));

        QByteArray name = pkt.mid(2, pkt.indexOf('\0', 2) - 2);
        QCOMPARE(name, QByteArray("upload.dat"));
    }

    void requestPacketUtf8Filename()
    {
        QString utf8Name = QString::fromUtf8("日本語ファイル.bin");
        QByteArray pkt = TftpClient::requestPacket(TftpClient::RRQ, utf8Name);

        QByteArray name = pkt.mid(2, pkt.indexOf('\0', 2) - 2);
        QCOMPARE(QString::fromUtf8(name), utf8Name);
    }

    void requestPacketEmptyFilename()
    {
        QByteArray pkt = TftpClient::requestPacket(TftpClient::RRQ, "");
        uint16_t op = qFromBigEndian<uint16_t>(
                          reinterpret_cast<const uchar *>(pkt.constData()));
        QCOMPARE(op, uint16_t(1));
        QCOMPARE(pkt.at(2), '\0');
    }

    // ---- ACK packets ----

    void ackPacketLayout()
    {
        QByteArray pkt = TftpClient::ackPacket(1);
        QCOMPARE(pkt.size(), 4);

        uint16_t op  = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData()));
        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));

        QCOMPARE(op,  uint16_t(4));
        QCOMPARE(blk, uint16_t(1));
    }

    void ackPacketBlockZero()
    {
        QByteArray pkt = TftpClient::ackPacket(0);
        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));
        QCOMPARE(blk, uint16_t(0));
    }

    void ackPacketBlockMax()
    {
        QByteArray pkt = TftpClient::ackPacket(0xFFFF);
        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));
        QCOMPARE(blk, uint16_t(0xFFFF));
    }

    void ackPacketVariousBlocks_data()
    {
        QTest::addColumn<uint16_t>("block");
        QTest::newRow("block 0")     << uint16_t(0);
        QTest::newRow("block 1")     << uint16_t(1);
        QTest::newRow("block 100")   << uint16_t(100);
        QTest::newRow("block 256")   << uint16_t(256);
        QTest::newRow("block 1000")  << uint16_t(1000);
        QTest::newRow("block 65535") << uint16_t(65535);
    }

    void ackPacketVariousBlocks()
    {
        QFETCH(uint16_t, block);
        QByteArray pkt = TftpClient::ackPacket(block);
        QCOMPARE(pkt.size(), 4);

        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));
        QCOMPARE(blk, block);
    }

    // ---- DATA packets ----

    void dataPacketLayout()
    {
        QByteArray payload("Hello, TFTP!");
        QByteArray pkt = TftpClient::dataPacket(1, payload);

        QCOMPARE(pkt.size(), 4 + payload.size());

        uint16_t op  = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData()));
        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));

        QCOMPARE(op,  uint16_t(3));
        QCOMPARE(blk, uint16_t(1));
        QCOMPARE(pkt.mid(4), payload);
    }

    void dataPacketEmpty()
    {
        QByteArray pkt = TftpClient::dataPacket(5, {});
        QCOMPARE(pkt.size(), 4);

        uint16_t blk = qFromBigEndian<uint16_t>(
                           reinterpret_cast<const uchar *>(pkt.constData() + 2));
        QCOMPARE(blk, uint16_t(5));
    }

    void dataPacketFullBlock()
    {
        QByteArray payload(512, 'X');
        QByteArray pkt = TftpClient::dataPacket(3, payload);
        QCOMPARE(pkt.size(), 4 + 512);
        QCOMPARE(pkt.mid(4), payload);
    }

    void dataPacketBinaryPayload()
    {
        QByteArray payload;
        for (int i = 0; i < 256; ++i)
            payload.append(static_cast<char>(i));
        QByteArray pkt = TftpClient::dataPacket(1, payload);
        QCOMPARE(pkt.mid(4), payload);
    }

    // ---- Cross-validation ----

    void roundtripConsistency()
    {
        for (uint16_t blk = 0; blk < 10; ++blk) {
            QByteArray ack  = TftpClient::ackPacket(blk);
            QByteArray data = TftpClient::dataPacket(blk, QByteArray(100, 'A'));

            uint16_t ackOp  = qFromBigEndian<uint16_t>(
                reinterpret_cast<const uchar *>(ack.constData()));
            uint16_t dataOp = qFromBigEndian<uint16_t>(
                reinterpret_cast<const uchar *>(data.constData()));

            QCOMPARE(ackOp,  uint16_t(4));
            QCOMPARE(dataOp, uint16_t(3));
            QVERIFY(ackOp != dataOp);
        }
    }
};

QTEST_GUILESS_MAIN(TestTftpPackets)
#include "test_tftp_packets.moc"
