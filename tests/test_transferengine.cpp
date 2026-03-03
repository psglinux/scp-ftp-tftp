#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "transferengine.h"

class TestTransferEngine : public QObject {
    Q_OBJECT

private slots:
    // ---- TransferConfig defaults ----

    void configDefaultProtocol()
    {
        TransferEngine::TransferConfig cfg;
        QCOMPARE(cfg.protocol, TransferEngine::SCP);
    }

    void configDefaultDirection()
    {
        TransferEngine::TransferConfig cfg;
        QCOMPARE(cfg.direction, TransferEngine::Upload);
    }

    void configDefaultPort()
    {
        TransferEngine::TransferConfig cfg;
        QCOMPARE(cfg.port, 22);
    }

    void configDefaultStringsEmpty()
    {
        TransferEngine::TransferConfig cfg;
        QVERIFY(cfg.host.isEmpty());
        QVERIFY(cfg.username.isEmpty());
        QVERIFY(cfg.password.isEmpty());
        QVERIFY(cfg.keyFile.isEmpty());
        QVERIFY(cfg.localPath.isEmpty());
        QVERIFY(cfg.remotePath.isEmpty());
    }

    // ---- buildCurlUrl ----

    void urlScpBasic()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.host       = "192.168.1.10";
        cfg.port       = 22;
        cfg.remotePath = "/home/user/file.bin";

        QCOMPARE(engine.buildCurlUrl(cfg),
                 QString("scp://192.168.1.10:22/home/user/file.bin"));
    }

    void urlFtpBasic()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::FTP;
        cfg.host       = "ftp.example.com";
        cfg.port       = 21;
        cfg.remotePath = "/pub/data.csv";

        QCOMPARE(engine.buildCurlUrl(cfg),
                 QString("ftp://ftp.example.com:21/pub/data.csv"));
    }

    void urlPrependsSlashWhenMissing()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.host       = "host";
        cfg.port       = 22;
        cfg.remotePath = "relative/path.txt";

        QString url = engine.buildCurlUrl(cfg);
        QVERIFY(url.contains("/relative/path.txt"));
        QCOMPARE(url, QString("scp://host:22/relative/path.txt"));
    }

    void urlPreservesLeadingSlash()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::FTP;
        cfg.host       = "srv";
        cfg.port       = 21;
        cfg.remotePath = "/already/absolute";

        QString url = engine.buildCurlUrl(cfg);
        QVERIFY(!url.contains("//already"));
        QCOMPARE(url, QString("ftp://srv:21/already/absolute"));
    }

    void urlCustomPort()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.host       = "myhost";
        cfg.port       = 2222;
        cfg.remotePath = "/file";

        QCOMPARE(engine.buildCurlUrl(cfg),
                 QString("scp://myhost:2222/file"));
    }

    void urlIpv4Host()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::FTP;
        cfg.host       = "10.0.0.1";
        cfg.port       = 21;
        cfg.remotePath = "/firmware.bin";

        QCOMPARE(engine.buildCurlUrl(cfg),
                 QString("ftp://10.0.0.1:21/firmware.bin"));
    }

    void urlSpecialCharsInPath()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.host       = "host";
        cfg.port       = 22;
        cfg.remotePath = "/path with spaces/file (1).txt";

        QString url = engine.buildCurlUrl(cfg);
        QVERIFY(url.contains("/path with spaces/file (1).txt"));
    }

    // ---- cancelTransfer ----

    void cancelSetsFlag()
    {
        TransferEngine engine;
        engine.cancelTransfer();

        TransferEngine::TransferConfig cfg;
        cfg.protocol  = TransferEngine::SCP;
        cfg.host      = "unreachable.invalid";
        cfg.port      = 22;
        cfg.localPath = "/nonexistent";
        cfg.remotePath = "/remote";
        cfg.direction  = TransferEngine::Upload;

        QSignalSpy spy(&engine, &TransferEngine::transferCompleted);
        engine.startTransfer(cfg);
        QVERIFY(spy.count() >= 1);
    }

    // ---- Signal tests: file-not-found ----

    void uploadNonexistentFileEmitsError()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.direction  = TransferEngine::Upload;
        cfg.host       = "localhost";
        cfg.port       = 22;
        cfg.localPath  = "/tmp/this_file_does_not_exist_12345.bin";
        cfg.remotePath = "/remote";

        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QList<QVariant> args = completedSpy.first();
        QCOMPARE(args.at(0).toBool(), false);
        QString errMsg = args.at(1).toString();
        QVERIFY(errMsg.contains("Cannot open") || errMsg.contains("SSH support"));
    }

    void downloadToUnwritablePathEmitsError()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::FTP;
        cfg.direction  = TransferEngine::Download;
        cfg.host       = "localhost";
        cfg.port       = 21;
        cfg.localPath  = "/proc/no_such_dir/file.bin";
        cfg.remotePath = "/remote";

        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QCOMPARE(completedSpy.first().at(0).toBool(), false);
    }

    // ---- Signal tests: log messages emitted ----

    void startTransferEmitsLogMessages()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.direction  = TransferEngine::Upload;
        cfg.host       = "localhost";
        cfg.port       = 22;
        cfg.localPath  = "/tmp/nonexistent_file_98765.bin";
        cfg.remotePath = "/remote";

        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);
        engine.startTransfer(cfg);

        QVERIFY(logSpy.count() >= 1);
        bool foundInit = false;
        for (const auto &call : logSpy) {
            if (call.first().toString().contains("Initializing"))
                foundInit = true;
        }
        QVERIFY(foundInit);
    }

    // ---- Signal tests: TFTP with nonexistent file ----

    void tftpUploadNonexistentFile()
    {
        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::TFTP;
        cfg.direction  = TransferEngine::Upload;
        cfg.host       = "127.0.0.1";
        cfg.port       = 69;
        cfg.localPath  = "/tmp/no_such_file_tftp_test.bin";
        cfg.remotePath = "/remote";

        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QCOMPARE(completedSpy.first().at(0).toBool(), false);
    }

    // ---- Protocol enum values ----

    void protocolEnumValues()
    {
        QCOMPARE(static_cast<int>(TransferEngine::SCP),  0);
        QCOMPARE(static_cast<int>(TransferEngine::FTP),  1);
        QCOMPARE(static_cast<int>(TransferEngine::TFTP), 2);
    }

    void directionEnumValues()
    {
        QCOMPARE(static_cast<int>(TransferEngine::Upload),   0);
        QCOMPARE(static_cast<int>(TransferEngine::Download), 1);
    }

    // ---- Engine construction / destruction ----

    void engineCreation()
    {
        TransferEngine *engine = new TransferEngine;
        QVERIFY(engine != nullptr);
        delete engine;
    }

    void engineWithParent()
    {
        QObject parent;
        auto *engine = new TransferEngine(&parent);
        QVERIFY(engine->parent() == &parent);
    }
};

QTEST_GUILESS_MAIN(TestTransferEngine)
#include "test_transferengine.moc"
