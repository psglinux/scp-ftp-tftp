#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include "transferengine.h"
#include "tftpclient.h"
#include "test_helpers.h"

static constexpr const char *SCP_HOST  = "127.0.0.1";
static constexpr int         SCP_PORT  = 2222;
static constexpr const char *FTP_HOST  = "127.0.0.1";
static constexpr int         FTP_PORT  = 2121;
static constexpr const char *TFTP_HOST = "127.0.0.1";
static constexpr int         TFTP_PORT = 6969;
static constexpr const char *USER      = "testuser";
static constexpr const char *PASS      = "testpass";

class TestPerformance : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmpDir_;

    struct BenchResult {
        QString protocol;
        QString size;
        double uploadSecs;
        double downloadSecs;
        double uploadMBps;
        double downloadMBps;
        bool ok;
    };

    QList<BenchResult> results_;

    void writeTestFile(const QString &path, qint64 size)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return;
        constexpr int CHUNK = 64 * 1024;
        QByteArray chunk = TestHelpers::generateTestData(CHUNK);
        qint64 remaining = size;
        while (remaining > 0) {
            qint64 toWrite = qMin(remaining, qint64(CHUNK));
            f.write(chunk.constData(), toWrite);
            remaining -= toWrite;
        }
    }

    BenchResult benchCurl(TransferEngine::Protocol proto,
                          const QString &protoName,
                          const QString &host, int port,
                          const QString &sizeLabel, qint64 fileSize)
    {
        BenchResult res{protoName, sizeLabel, 0, 0, 0, 0, false};

        if (!TestHelpers::isTcpPortOpen(host, port)) {
            qWarning() << protoName << "server not available, skipping" << sizeLabel;
            return res;
        }

        QString localSrc = tmpDir_.filePath(
            QStringLiteral("perf_%1_%2_src.bin").arg(protoName, sizeLabel));
        writeTestFile(localSrc, fileSize);

        TransferEngine engine;
        TransferEngine::TransferConfig cfg;
        cfg.protocol  = proto;
        cfg.host      = host;
        cfg.port      = port;
        cfg.username  = USER;
        cfg.password  = PASS;

        QString remoteName = QStringLiteral("perf_%1_%2.bin")
                                 .arg(protoName, sizeLabel);
        QString remotePath = (proto == TransferEngine::SCP)
            ? "/config/" + remoteName : "/" + remoteName;

        cfg.direction  = TransferEngine::Upload;
        cfg.localPath  = localSrc;
        cfg.remotePath = remotePath;

        QSignalSpy ulSpy(&engine, &TransferEngine::transferCompleted);
        QElapsedTimer t;
        t.start();
        engine.startTransfer(cfg);
        res.uploadSecs = t.elapsed() / 1000.0;

        if (ulSpy.isEmpty() || !ulSpy.first().at(0).toBool()) {
            qWarning() << protoName << sizeLabel << "upload FAILED:"
                       << (ulSpy.isEmpty() ? "no signal"
                                           : ulSpy.first().at(1).toString());
            return res;
        }

        QString localDst = tmpDir_.filePath(
            QStringLiteral("perf_%1_%2_dst.bin").arg(protoName, sizeLabel));
        cfg.direction = TransferEngine::Download;
        cfg.localPath = localDst;

        QSignalSpy dlSpy(&engine, &TransferEngine::transferCompleted);
        t.restart();
        engine.startTransfer(cfg);
        res.downloadSecs = t.elapsed() / 1000.0;

        if (dlSpy.isEmpty() || !dlSpy.first().at(0).toBool()) {
            qWarning() << protoName << sizeLabel << "download FAILED:"
                       << (dlSpy.isEmpty() ? "no signal"
                                           : dlSpy.first().at(1).toString());
            return res;
        }

        double sizeMB = fileSize / (1024.0 * 1024.0);
        res.uploadMBps   = (res.uploadSecs > 0) ? sizeMB / res.uploadSecs : 0;
        res.downloadMBps = (res.downloadSecs > 0) ? sizeMB / res.downloadSecs : 0;
        res.ok = true;

        if (TestHelpers::fileChecksum(localSrc) != TestHelpers::fileChecksum(localDst)) {
            qWarning() << protoName << sizeLabel << "checksum mismatch!";
            res.ok = false;
        }

        return res;
    }

    BenchResult benchTftp(const QString &sizeLabel, qint64 fileSize)
    {
        BenchResult res{"TFTP", sizeLabel, 0, 0, 0, 0, false};

        if (!TestHelpers::isUdpServiceUp(TFTP_HOST, TFTP_PORT)) {
            qWarning() << "TFTP server not available, skipping" << sizeLabel;
            return res;
        }

        QString localSrc = tmpDir_.filePath(
            QStringLiteral("perf_tftp_%1_src.bin").arg(sizeLabel));
        writeTestFile(localSrc, fileSize);

        std::atomic<bool> cancel{false};
        QString remoteName = QStringLiteral("perf_tftp_%1.bin").arg(sizeLabel);

        TftpClient upClient;
        QElapsedTimer t;
        t.start();
        bool ulOk = upClient.upload(TFTP_HOST, TFTP_PORT,
                                    localSrc, remoteName, &cancel);
        res.uploadSecs = t.elapsed() / 1000.0;

        if (!ulOk) {
            qWarning() << "TFTP" << sizeLabel << "upload FAILED";
            return res;
        }

        QString localDst = tmpDir_.filePath(
            QStringLiteral("perf_tftp_%1_dst.bin").arg(sizeLabel));
        TftpClient dlClient;
        t.restart();
        bool dlOk = dlClient.download(TFTP_HOST, TFTP_PORT,
                                      remoteName, localDst, &cancel);
        res.downloadSecs = t.elapsed() / 1000.0;

        if (!dlOk) {
            qWarning() << "TFTP" << sizeLabel << "download FAILED";
            return res;
        }

        double sizeMB = fileSize / (1024.0 * 1024.0);
        res.uploadMBps   = (res.uploadSecs > 0) ? sizeMB / res.uploadSecs : 0;
        res.downloadMBps = (res.downloadSecs > 0) ? sizeMB / res.downloadSecs : 0;
        res.ok = true;

        if (TestHelpers::fileChecksum(localSrc) != TestHelpers::fileChecksum(localDst)) {
            qWarning() << "TFTP" << sizeLabel << "checksum mismatch!";
            res.ok = false;
        }

        return res;
    }

    void printResults()
    {
        qInfo().noquote() << "";
        qInfo().noquote() << "====== PERFORMANCE BENCHMARK RESULTS ======";
        qInfo().noquote() << QString("%1 | %2 | %3 | %4 | %5 | %6")
                       .arg("Proto", -6).arg("Size", -8)
                       .arg("UL (s)", -10).arg("DL (s)", -10)
                       .arg("UL MB/s", -10).arg("DL MB/s", -10);
        qInfo().noquote() << QString(64, '-');
        for (const auto &r : results_) {
            if (r.ok) {
                qInfo().noquote() << QString("%1 | %2 | %3 | %4 | %5 | %6")
                               .arg(r.protocol, -6).arg(r.size, -8)
                               .arg(r.uploadSecs, -10, 'f', 3)
                               .arg(r.downloadSecs, -10, 'f', 3)
                               .arg(r.uploadMBps, -10, 'f', 2)
                               .arg(r.downloadMBps, -10, 'f', 2);
            } else {
                qInfo().noquote() << QString("%1 | %2 | FAILED / SKIPPED")
                               .arg(r.protocol, -6).arg(r.size, -8);
            }
        }
        qInfo().noquote() << "============================================";
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmpDir_.isValid());
        bool hasScp  = TestHelpers::isTcpPortOpen(SCP_HOST, SCP_PORT);
        bool hasFtp  = TestHelpers::isTcpPortOpen(FTP_HOST, FTP_PORT);
        if (!hasScp && !hasFtp)
            QSKIP("No Docker test servers running -- skipping benchmarks");
    }

    void benchScp1KB()   { results_ << benchCurl(TransferEngine::SCP, "SCP", SCP_HOST, SCP_PORT, "1KB",   1024); }
    void benchScp1MB()   { results_ << benchCurl(TransferEngine::SCP, "SCP", SCP_HOST, SCP_PORT, "1MB",   1024*1024); }
    void benchScp10MB()  { results_ << benchCurl(TransferEngine::SCP, "SCP", SCP_HOST, SCP_PORT, "10MB",  10*1024*1024); }
    void benchScp100MB() { results_ << benchCurl(TransferEngine::SCP, "SCP", SCP_HOST, SCP_PORT, "100MB", 100LL*1024*1024); }
    void benchScp1GB()   { results_ << benchCurl(TransferEngine::SCP, "SCP", SCP_HOST, SCP_PORT, "1GB",   1024LL*1024*1024); }

    void benchFtp1KB()   { results_ << benchCurl(TransferEngine::FTP, "FTP", FTP_HOST, FTP_PORT, "1KB",   1024); }
    void benchFtp1MB()   { results_ << benchCurl(TransferEngine::FTP, "FTP", FTP_HOST, FTP_PORT, "1MB",   1024*1024); }
    void benchFtp10MB()  { results_ << benchCurl(TransferEngine::FTP, "FTP", FTP_HOST, FTP_PORT, "10MB",  10*1024*1024); }
    void benchFtp100MB() { results_ << benchCurl(TransferEngine::FTP, "FTP", FTP_HOST, FTP_PORT, "100MB", 100LL*1024*1024); }
    void benchFtp1GB()   { results_ << benchCurl(TransferEngine::FTP, "FTP", FTP_HOST, FTP_PORT, "1GB",   1024LL*1024*1024); }

    void benchTftp1KB()  { results_ << benchTftp("1KB",  1024); }
    void benchTftp1MB()  { results_ << benchTftp("1MB",  1024*1024); }
    void benchTftp10MB() { results_ << benchTftp("10MB", 10*1024*1024); }

    void cleanupTestCase()
    {
        printResults();
    }
};

QTEST_GUILESS_MAIN(TestPerformance)
#include "test_performance.moc"
