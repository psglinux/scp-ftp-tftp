#ifndef TRANSFERENGINE_H
#define TRANSFERENGINE_H

#include <QObject>
#include <QString>
#include <atomic>

class TransferEngine : public QObject {
    Q_OBJECT

public:
    enum Protocol  { SCP = 0, FTP = 1, TFTP = 2 };
    enum Direction { Upload, Download };

    struct TransferConfig {
        Protocol  protocol  = SCP;
        Direction direction = Upload;
        QString   host;
        int       port      = 22;
        QString   username;
        QString   password;
        QString   keyFile;
        QString   localPath;
        QString   remotePath;
    };

    explicit TransferEngine(QObject *parent = nullptr);
    ~TransferEngine() override;

    void startTransfer(const TransferConfig &config);
    void cancelTransfer();
    QString buildCurlUrl(const TransferConfig &config) const;

signals:
    void progressChanged(qint64 bytesTransferred, qint64 totalBytes);
    void transferCompleted(bool success, const QString &message);
    void logMessage(const QString &message);

private:
    void transferWithCurl(const TransferConfig &config);
    void transferWithTftp(const TransferConfig &config);

    std::atomic<bool> cancelled_{false};
};

#endif // TRANSFERENGINE_H
