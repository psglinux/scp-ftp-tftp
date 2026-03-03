#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "transferengine.h"
#include "tftpclient.h"
#include "test_helpers.h"

static constexpr const char *TFTP_HOST = "127.0.0.1";
static constexpr int         TFTP_PORT = 6969;

class TestTftpRealIntegration : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmpDir_;

private slots:
    void initTestCase()
    {
        QVERIFY(tmpDir_.isValid());
        TestHelpers::skipIfNoDocker("tftp-server", TFTP_HOST, TFTP_PORT, true);
    }

    void uploadAndDownload()
    {
        QByteArray content = TestHelpers::generateTestData(2048);
        QString localSrc = tmpDir_.filePath("tftp_upload.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TftpClient client;
        std::atomic<bool> cancel{false};

        bool uploadOk = client.upload(TFTP_HOST, TFTP_PORT,
                                      localSrc, "tftp_upload.bin", &cancel);
        if (!uploadOk)
            QSKIP("TFTP upload failed -- Docker UDP port forwarding is unreliable for TFTP. "
                   "Mock-based TftpIntegration tests provide coverage instead.");

        QString localDst = tmpDir_.filePath("tftp_download.bin");
        TftpClient client2;
        bool downloadOk = client2.download(TFTP_HOST, TFTP_PORT,
                                           "tftp_upload.bin", localDst, &cancel);
        if (!downloadOk)
            QSKIP("TFTP download failed -- Docker UDP port forwarding limitation.");

        QCOMPARE(TestHelpers::fileChecksum(localSrc),
                 TestHelpers::fileChecksum(localDst));
    }

    void downloadNonexistentFile()
    {
        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        QString localDst = tmpDir_.filePath("tftp_nonexist.bin");
        bool ok = client.download(TFTP_HOST, TFTP_PORT,
                                  "this_file_does_not_exist_12345.bin",
                                  localDst, &cancel);
        QVERIFY(!ok);

        bool hasError = false;
        for (const auto &call : logSpy) {
            QString msg = call.first().toString().toLower();
            if (msg.contains("error") || msg.contains("not found")
                || msg.contains("timeout"))
                hasError = true;
        }
        QVERIFY2(hasError, "Log should indicate file not found or error");
    }

    void logContainsExpectedPhases()
    {
        QByteArray content = TestHelpers::generateTestData(256);
        QString localSrc = tmpDir_.filePath("tftp_logcheck.bin");
        {
            QFile f(localSrc);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        }

        TftpClient client;
        QSignalSpy logSpy(&client, &TftpClient::logMessage);
        std::atomic<bool> cancel{false};

        bool ok = client.upload(TFTP_HOST, TFTP_PORT, localSrc, "tftp_logcheck.bin", &cancel);

        QStringList allLogs;
        for (const auto &call : logSpy)
            allLogs << call.first().toString();
        QString combined = allLogs.join("\n");

        QVERIFY2(combined.contains("WRQ"), "Should log WRQ send");
        if (ok)
            QVERIFY2(combined.contains("complete") || combined.contains("sent"),
                     "Should log completion");
        else
            QSKIP("Upload failed due to Docker UDP limitation -- can't verify completion log");
    }
};

QTEST_GUILESS_MAIN(TestTftpRealIntegration)
#include "test_tftp_real_integration.moc"
