#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <QProcess>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QtTest>

namespace TestHelpers {

inline bool isTcpPortOpen(const QString &host, quint16 port, int timeoutMs = 2000)
{
    QTcpSocket sock;
    sock.connectToHost(host, port);
    return sock.waitForConnected(timeoutMs);
}

inline bool isUdpServiceUp(const QString & /*host*/, quint16 /*port*/)
{
    QProcess proc;
    proc.start("docker", {"inspect", "--format", "{{.State.Running}}",
                           "gotikinesis-tftp"});
    proc.waitForFinished(3000);
    return proc.readAllStandardOutput().trimmed() == "true";
}

inline void skipIfNoDocker(const QString &service, const QString &host, quint16 port, bool udp = false)
{
    bool up = udp ? isUdpServiceUp(host, port) : isTcpPortOpen(host, port);
    if (!up)
        QSKIP(qPrintable(
            QStringLiteral("Docker %1 not running on %2:%3 -- skipping. "
                           "Run: cd tests && docker compose up -d")
                .arg(service, host)
                .arg(port)));
}

inline QByteArray generateTestData(int size)
{
    QByteArray data(size, '\0');
    for (int i = 0; i < size; ++i)
        data[i] = static_cast<char>(i % 251);
    return data;
}

inline QByteArray fileChecksum(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

inline QString sshTestKeyPath()
{
    QString path = QStringLiteral(TESTS_SOURCE_DIR) + "/docker/ssh_test_key";
    if (QFile::exists(path))
        return path;
    return {};
}

} // namespace TestHelpers

#endif // TEST_HELPERS_H
