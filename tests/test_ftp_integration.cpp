#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "transferengine.h"
#include "test_helpers.h"

static constexpr const char *FTP_HOST = "127.0.0.1";
static constexpr int         FTP_PORT = 2121;
static constexpr const char *FTP_USER = "testuser";
static constexpr const char *FTP_PASS = "testpass";

class TestFtpIntegration : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmpDir_;

    TransferEngine::TransferConfig makeConfig(
        TransferEngine::Direction dir,
        const QString &localPath,
        const QString &remotePath)
    {
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::FTP;
        cfg.direction  = dir;
        cfg.host       = FTP_HOST;
        cfg.port       = FTP_PORT;
        cfg.username   = FTP_USER;
        cfg.password   = FTP_PASS;
        cfg.localPath  = localPath;
        cfg.remotePath = remotePath;
        return cfg;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmpDir_.isValid());
        TestHelpers::skipIfNoDocker("ftp-server", FTP_HOST, FTP_PORT);
    }

    void uploadWithCredentials()
    {
        QByteArray content = TestHelpers::generateTestData(4096);
        QString localSrc = tmpDir_.filePath("ftp_upload.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/ftp_upload.bin");
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QVERIFY2(completedSpy.first().at(0).toBool(),
                 qPrintable(completedSpy.first().at(1).toString()));

        // Download back and verify
        QString localDst = tmpDir_.filePath("ftp_download.bin");
        QSignalSpy dlSpy(&engine, &TransferEngine::transferCompleted);
        auto dlCfg = makeConfig(TransferEngine::Download, localDst,
                                "/ftp_upload.bin");
        engine.startTransfer(dlCfg);

        QVERIFY(dlSpy.count() >= 1);
        QVERIFY2(dlSpy.first().at(0).toBool(),
                 qPrintable(dlSpy.first().at(1).toString()));

        QCOMPARE(TestHelpers::fileChecksum(localSrc),
                 TestHelpers::fileChecksum(localDst));
    }

    void downloadWithCredentials()
    {
        QByteArray content = TestHelpers::generateTestData(2048);
        QString localSrc = tmpDir_.filePath("ftp_seed.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy ulSpy(&engine, &TransferEngine::transferCompleted);
        auto ulCfg = makeConfig(TransferEngine::Upload, localSrc,
                                "/ftp_seed.bin");
        engine.startTransfer(ulCfg);
        QVERIFY(ulSpy.count() >= 1);
        QVERIFY2(ulSpy.first().at(0).toBool(),
                 qPrintable(ulSpy.first().at(1).toString()));

        QString localDst = tmpDir_.filePath("ftp_dl_verify.bin");
        QSignalSpy dlSpy(&engine, &TransferEngine::transferCompleted);
        auto dlCfg = makeConfig(TransferEngine::Download, localDst,
                                "/ftp_seed.bin");
        engine.startTransfer(dlCfg);

        QVERIFY(dlSpy.count() >= 1);
        QVERIFY2(dlSpy.first().at(0).toBool(),
                 qPrintable(dlSpy.first().at(1).toString()));

        QFile df(localDst);
        QVERIFY(df.open(QIODevice::ReadOnly));
        QCOMPARE(df.readAll(), content);
    }

    void authFailure()
    {
        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        QString localSrc = tmpDir_.filePath("ftp_authfail.bin");
        { QFile f(localSrc); f.open(QIODevice::WriteOnly); f.write("x"); }

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/ftp_authfail.bin");
        cfg.password = "wrongpassword";
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QCOMPARE(completedSpy.first().at(0).toBool(), false);

        bool hasAuthError = false;
        for (const auto &call : logSpy) {
            QString msg = call.first().toString().toLower();
            if (msg.contains("530") || msg.contains("login")
                || msg.contains("denied") || msg.contains("auth")
                || msg.contains("access"))
                hasAuthError = true;
        }
        QVERIFY2(hasAuthError, "Log should contain FTP auth error (530 or similar)");
    }

    void connectionRefused()
    {
        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        QString localSrc = tmpDir_.filePath("ftp_connfail.bin");
        { QFile f(localSrc); f.open(QIODevice::WriteOnly); f.write("x"); }

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/ftp_connfail.bin");
        cfg.port = 29998;
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QCOMPARE(completedSpy.first().at(0).toBool(), false);

        bool hasConnError = false;
        for (const auto &call : logSpy) {
            QString msg = call.first().toString().toLower();
            if (msg.contains("connect") || msg.contains("refused")
                || msg.contains("error") || msg.contains("failed"))
                hasConnError = true;
        }
        QVERIFY2(hasConnError, "Log should contain connection error");
    }

    void logContainsFtpDialogue()
    {
        QByteArray content = TestHelpers::generateTestData(512);
        QString localSrc = tmpDir_.filePath("ftp_logcheck.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/ftp_logcheck.bin");
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);

        QStringList allLogs;
        for (const auto &call : logSpy)
            allLogs << call.first().toString();
        QString combined = allLogs.join("\n").toLower();

        QVERIFY2(combined.contains("initializing"), "Should log initialization");
        QVERIFY2(combined.contains("connecting"), "Should log connection");
        QVERIFY2(combined.contains("curl"), "Should contain curl debug output");
        QVERIFY2(combined.contains("user") || combined.contains("login"),
                 "Should log FTP USER command or login");
    }
};

QTEST_GUILESS_MAIN(TestFtpIntegration)
#include "test_ftp_integration.moc"
