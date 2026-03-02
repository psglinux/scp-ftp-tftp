#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "tftpclient.h"
#include "mock_tftp_server.h"

class TestTftpIntegration : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmpDir_;

    QString tmpFile(const QString &name) const
    {
        return tmpDir_.filePath(name);
    }

    void writeFile(const QString &path, const QByteArray &data)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
        f.close();
    }

    QByteArray readFile(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    // Generate repeatable test content of the given size.
    static QByteArray makeContent(int size)
    {
        QByteArray data(size, '\0');
        for (int i = 0; i < size; ++i)
            data[i] = static_cast<char>(i % 251);
        return data;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(tmpDir_.isValid());
    }

    // ---- Download tests ----

    void downloadSmallFile()
    {
        QByteArray content = makeContent(200);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_small.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "test.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath), content);
    }

    void downloadExactOneBlock()
    {
        QByteArray content = makeContent(512);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_512.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "exact.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath), content);
    }

    void downloadMultiBlock()
    {
        QByteArray content = makeContent(1500);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_multi.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "multi.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath), content);
    }

    void downloadEmptyFile()
    {
        QByteArray content;
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_empty.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "empty.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath).size(), 0);
    }

    void downloadLargeFile()
    {
        QByteArray content = makeContent(5000);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_large.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "large.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath), content);
    }

    void downloadEmitsProgress()
    {
        QByteArray content = makeContent(1500);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy progressSpy(&client, &TftpClient::progressChanged);
        std::atomic<bool> cancel{false};

        client.download("127.0.0.1", srv.serverPort(),
                        "prog.bin", tmpFile("dl_prog.bin"), &cancel);
        srv.wait(5000);

        QVERIFY(progressSpy.count() >= 2);

        qint64 lastTransferred = 0;
        for (const auto &call : progressSpy) {
            qint64 t = call.at(0).toLongLong();
            QVERIFY(t >= lastTransferred);
            lastTransferred = t;
        }
    }

    void downloadEmitsLogMessages()
    {
        QByteArray content = makeContent(100);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        client.download("127.0.0.1", srv.serverPort(),
                        "log.bin", tmpFile("dl_log.bin"), &cancel);
        srv.wait(5000);

        QVERIFY(logSpy.count() >= 2);
        QVERIFY(logSpy.first().first().toString().contains("RRQ"));
    }

    // ---- Upload tests ----

    void uploadSmallFile()
    {
        QByteArray content = makeContent(300);
        writeFile(tmpFile("ul_small.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_small.bin"), "remote.bin", &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(srv.receivedData(), content);
    }

    void uploadExactOneBlock()
    {
        QByteArray content = makeContent(512);
        writeFile(tmpFile("ul_512.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_512.bin"), "exact.bin", &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(srv.receivedData(), content);
    }

    void uploadMultiBlock()
    {
        QByteArray content = makeContent(2000);
        writeFile(tmpFile("ul_multi.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_multi.bin"), "multi.bin", &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(srv.receivedData(), content);
    }

    void uploadEmptyFile()
    {
        writeFile(tmpFile("ul_empty.bin"), {});

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_empty.bin"), "empty.bin", &cancel);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(srv.receivedData().size(), 0);
    }

    void uploadEmitsProgress()
    {
        QByteArray content = makeContent(1500);
        writeFile(tmpFile("ul_prog.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy progressSpy(&client, &TftpClient::progressChanged);
        std::atomic<bool> cancel{false};

        client.upload("127.0.0.1", srv.serverPort(),
                      tmpFile("ul_prog.bin"), "prog.bin", &cancel);
        srv.wait(5000);

        QVERIFY(progressSpy.count() >= 2);

        qint64 last = 0;
        for (const auto &call : progressSpy) {
            qint64 t = call.at(0).toLongLong();
            QVERIFY(t >= last);
            last = t;
        }
        QCOMPARE(last, qint64(content.size()));
    }

    // ---- Error handling ----

    void downloadServerError()
    {
        MockTftpServer srv;
        srv.configure(MockTftpServer::SendError, {},
                      1, "File not found");
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "missing.bin", tmpFile("dl_err.bin"),
                                  &cancel);
        srv.wait(5000);

        QVERIFY(!ok);

        bool foundError = false;
        for (const auto &call : logSpy) {
            if (call.first().toString().contains("File not found"))
                foundError = true;
        }
        QVERIFY(foundError);
    }

    void uploadServerError()
    {
        writeFile(tmpFile("ul_err.bin"), makeContent(100));

        MockTftpServer srv;
        srv.configure(MockTftpServer::SendError, {},
                      2, "Access violation");
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_err.bin"), "denied.bin", &cancel);
        srv.wait(5000);

        QVERIFY(!ok);
    }

    void downloadNonexistentLocalPath()
    {
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, makeContent(100));
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "test.bin",
                                  "/no_such_directory/file.bin", &cancel);

        srv.requestInterruption();
        srv.wait(5000);

        QVERIFY(!ok);
    }

    void uploadNonexistentLocalFile()
    {
        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        bool ok = client.upload("127.0.0.1", 12345,
                                "/tmp/absolutely_no_such_file.bin",
                                "remote.bin", &cancel);

        QVERIFY(!ok);
        QVERIFY(logSpy.count() >= 1);
        QVERIFY(logSpy.first().first().toString().contains("Cannot open"));
    }

    // ---- Cancellation ----

    void downloadCancellation()
    {
        QByteArray content = makeContent(5000);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};
        QString outPath = tmpFile("dl_cancel.bin");

        QObject::connect(&client, &TftpClient::progressChanged,
                         [&cancel](qint64, qint64) {
            cancel.store(true);
        });

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "big.bin", outPath, &cancel);
        srv.wait(5000);

        QVERIFY(!ok);
        QVERIFY(!QFile::exists(outPath));
    }

    void uploadCancellation()
    {
        QByteArray content = makeContent(5000);
        writeFile(tmpFile("ul_cancel.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        std::atomic<bool> cancel{false};

        QObject::connect(&client, &TftpClient::progressChanged,
                         [&cancel](qint64, qint64) {
            cancel.store(true);
        });

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_cancel.bin"), "big.bin", &cancel);
        srv.wait(5000);

        QVERIFY(!ok);
    }

    // ---- Null cancelled pointer ----

    void downloadWithNullCancelled()
    {
        QByteArray content = makeContent(100);
        MockTftpServer srv;
        srv.configure(MockTftpServer::ServeDownload, content);
        srv.start();
        srv.waitForReady();

        TftpClient client;
        QString outPath = tmpFile("dl_null_cancel.bin");

        bool ok = client.download("127.0.0.1", srv.serverPort(),
                                  "test.bin", outPath, nullptr);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(readFile(outPath), content);
    }

    void uploadWithNullCancelled()
    {
        QByteArray content = makeContent(100);
        writeFile(tmpFile("ul_null_cancel.bin"), content);

        MockTftpServer srv;
        srv.configure(MockTftpServer::AcceptUpload);
        srv.start();
        srv.waitForReady();

        TftpClient client;

        bool ok = client.upload("127.0.0.1", srv.serverPort(),
                                tmpFile("ul_null_cancel.bin"), "test.bin",
                                nullptr);
        srv.wait(5000);

        QVERIFY(ok);
        QCOMPARE(srv.receivedData(), content);
    }
};

QTEST_GUILESS_MAIN(TestTftpIntegration)
#include "moc_mock_tftp_server.cpp"
#include "test_tftp_integration.moc"
