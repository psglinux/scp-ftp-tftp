#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "transferengine.h"
#include "test_helpers.h"

static constexpr const char *SCP_HOST = "127.0.0.1";
static constexpr int         SCP_PORT = 2222;
static constexpr const char *SCP_USER = "testuser";
static constexpr const char *SCP_PASS = "testpass";

class TestScpIntegration : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmpDir_;

    TransferEngine::TransferConfig makeConfig(
        TransferEngine::Direction dir,
        const QString &localPath,
        const QString &remotePath)
    {
        TransferEngine::TransferConfig cfg;
        cfg.protocol   = TransferEngine::SCP;
        cfg.direction  = dir;
        cfg.host       = SCP_HOST;
        cfg.port       = SCP_PORT;
        cfg.username   = SCP_USER;
        cfg.password   = SCP_PASS;
        cfg.localPath  = localPath;
        cfg.remotePath = remotePath;
        return cfg;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmpDir_.isValid());
        TestHelpers::skipIfNoDocker("scp-server", SCP_HOST, SCP_PORT);
    }

    void uploadWithPassword()
    {
        QByteArray content = TestHelpers::generateTestData(4096);
        QString localSrc = tmpDir_.filePath("scp_upload.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/config/scp_upload.bin");
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QVERIFY2(completedSpy.first().at(0).toBool(),
                 qPrintable(completedSpy.first().at(1).toString()));

        // Download it back and verify
        QString localDst = tmpDir_.filePath("scp_download.bin");
        auto dlCfg = makeConfig(TransferEngine::Download, localDst,
                                "/config/scp_upload.bin");
        QSignalSpy dlSpy(&engine, &TransferEngine::transferCompleted);
        engine.startTransfer(dlCfg);

        QVERIFY(dlSpy.count() >= 1);
        QVERIFY2(dlSpy.first().at(0).toBool(),
                 qPrintable(dlSpy.first().at(1).toString()));

        QCOMPARE(TestHelpers::fileChecksum(localSrc),
                 TestHelpers::fileChecksum(localDst));
    }

    void downloadWithPassword()
    {
        QByteArray content = TestHelpers::generateTestData(2048);
        QString localSrc = tmpDir_.filePath("scp_seed.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy ulSpy(&engine, &TransferEngine::transferCompleted);
        auto ulCfg = makeConfig(TransferEngine::Upload, localSrc,
                                "/config/scp_seed.bin");
        engine.startTransfer(ulCfg);
        QVERIFY(ulSpy.count() >= 1);
        QVERIFY2(ulSpy.first().at(0).toBool(),
                 qPrintable(ulSpy.first().at(1).toString()));

        QString localDst = tmpDir_.filePath("scp_dl_verify.bin");
        QSignalSpy dlSpy(&engine, &TransferEngine::transferCompleted);
        auto dlCfg = makeConfig(TransferEngine::Download, localDst,
                                "/config/scp_seed.bin");
        engine.startTransfer(dlCfg);

        QVERIFY(dlSpy.count() >= 1);
        QVERIFY2(dlSpy.first().at(0).toBool(),
                 qPrintable(dlSpy.first().at(1).toString()));

        QFile df(localDst);
        QVERIFY(df.open(QIODevice::ReadOnly));
        QCOMPARE(df.readAll(), content);
    }

    void uploadWithKeyAuth()
    {
        QString keyPath = TestHelpers::sshTestKeyPath();
        if (keyPath.isEmpty())
            QSKIP("SSH test key not found");

        QByteArray content = TestHelpers::generateTestData(1024);
        QString localSrc = tmpDir_.filePath("scp_key_upload.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/config/scp_key_upload.bin");
        cfg.password.clear();
        cfg.keyFile = keyPath;
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QVERIFY2(completedSpy.first().at(0).toBool(),
                 qPrintable(completedSpy.first().at(1).toString()));
    }

    void authFailure()
    {
        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        QString localSrc = tmpDir_.filePath("scp_authfail.bin");
        { QFile f(localSrc); f.open(QIODevice::WriteOnly); f.write("x"); }

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/config/scp_authfail.bin");
        cfg.password = "wrongpassword";
        cfg.keyFile.clear();
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);
        QCOMPARE(completedSpy.first().at(0).toBool(), false);

        bool hasAuthError = false;
        for (const auto &call : logSpy) {
            QString msg = call.first().toString().toLower();
            if (msg.contains("auth") || msg.contains("denied")
                || msg.contains("password") || msg.contains("failed"))
                hasAuthError = true;
        }
        QVERIFY2(hasAuthError, "Log should contain authentication error details");
    }

    void connectionRefused()
    {
        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        QString localSrc = tmpDir_.filePath("scp_connfail.bin");
        { QFile f(localSrc); f.open(QIODevice::WriteOnly); f.write("x"); }

        auto cfg = makeConfig(TransferEngine::Upload, localSrc, "/config/x.bin");
        cfg.port = 29999;
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
        QVERIFY2(hasConnError, "Log should contain connection error details");
    }

    void logContainsExpectedPhases()
    {
        QByteArray content = TestHelpers::generateTestData(512);
        QString localSrc = tmpDir_.filePath("scp_logcheck.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TransferEngine engine;
        QSignalSpy completedSpy(&engine, &TransferEngine::transferCompleted);
        QSignalSpy logSpy(&engine, &TransferEngine::logMessage);

        auto cfg = makeConfig(TransferEngine::Upload, localSrc,
                              "/config/scp_logcheck.bin");
        engine.startTransfer(cfg);

        QVERIFY(completedSpy.count() >= 1);

        QStringList allLogs;
        for (const auto &call : logSpy)
            allLogs << call.first().toString();
        QString combined = allLogs.join("\n").toLower();

        QVERIFY2(combined.contains("initializing"), "Should log initialization");
        QVERIFY2(combined.contains("connecting"), "Should log connection attempt");
        QVERIFY2(combined.contains("curl"), "Should contain curl debug output");
        QVERIFY2(combined.contains("auth"), "Should log auth details");
    }
};

QTEST_GUILESS_MAIN(TestScpIntegration)
#include "test_scp_integration.moc"
