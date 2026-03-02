#include "transferengine.h"
#include "tftpclient.h"

#include <QFile>
#include <QFileInfo>
#include <curl/curl.h>

// ---------------------------------------------------------------------------
// libcurl callback context
// ---------------------------------------------------------------------------
struct CurlContext {
    TransferEngine   *engine;
    QFile            *file;
    std::atomic<bool>*cancelled;
};

static size_t curlWriteCb(void *ptr, size_t size, size_t nmemb, void *ud)
{
    auto *ctx = static_cast<CurlContext *>(ud);
    qint64 written = ctx->file->write(static_cast<const char *>(ptr),
                                      static_cast<qint64>(size * nmemb));
    return (written < 0) ? 0 : static_cast<size_t>(written);
}

static size_t curlReadCb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    auto *ctx = static_cast<CurlContext *>(ud);
    qint64 n  = ctx->file->read(ptr, static_cast<qint64>(size * nmemb));
    return (n < 0) ? CURL_READFUNC_ABORT : static_cast<size_t>(n);
}

static int curlProgressCb(void *clientp,
                           curl_off_t dltotal, curl_off_t dlnow,
                           curl_off_t ultotal, curl_off_t ulnow)
{
    auto *ctx  = static_cast<CurlContext *>(clientp);
    qint64 total = (dltotal > 0) ? static_cast<qint64>(dltotal)
                                 : static_cast<qint64>(ultotal);
    qint64 now   = (dlnow > 0)  ? static_cast<qint64>(dlnow)
                                 : static_cast<qint64>(ulnow);
    emit ctx->engine->progressChanged(now, total);
    return ctx->cancelled->load() ? 1 : 0;
}

static int sshHostKeyCb(CURL * /*easy*/,
                         const struct curl_khkey * /*knownkey*/,
                         const struct curl_khkey * /*foundkey*/,
                         enum curl_khmatch /*match*/,
                         void * /*clientp*/)
{
    return CURLKHSTAT_FINE;
}

// ---------------------------------------------------------------------------
// TransferEngine
// ---------------------------------------------------------------------------
TransferEngine::TransferEngine(QObject *parent)
    : QObject(parent) {}

TransferEngine::~TransferEngine() = default;

void TransferEngine::cancelTransfer()
{
    cancelled_.store(true);
}

void TransferEngine::startTransfer(const TransferConfig &config)
{
    cancelled_.store(false);

    if (config.protocol == TFTP)
        transferWithTftp(config);
    else
        transferWithCurl(config);
}

// ---------------------------------------------------------------------------
// SCP / FTP via libcurl
// ---------------------------------------------------------------------------
QString TransferEngine::buildCurlUrl(const TransferConfig &config) const
{
    QString scheme = (config.protocol == SCP) ? "scp" : "ftp";
    QString path   = config.remotePath;
    if (!path.startsWith('/'))
        path.prepend('/');

    return QStringLiteral("%1://%2:%3%4")
               .arg(scheme)
               .arg(config.host)
               .arg(config.port)
               .arg(path);
}

void TransferEngine::transferWithCurl(const TransferConfig &config)
{
    const char *protoName = (config.protocol == SCP) ? "SCP" : "FTP";
    emit logMessage(QStringLiteral("Initializing %1 transfer…").arg(protoName));

    CURL *curl = curl_easy_init();
    if (!curl) {
        emit transferCompleted(false, "Failed to initialize libcurl.");
        return;
    }

    QFile file(config.localPath);
    CurlContext ctx{this, &file, &cancelled_};

    QString url = buildCurlUrl(config);
    emit logMessage("URL: " + url);

    if (config.direction == Upload) {
        if (!file.open(QIODevice::ReadOnly)) {
            emit transferCompleted(false,
                "Cannot open local file for reading: " + config.localPath);
            curl_easy_cleanup(curl);
            return;
        }
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlReadCb);
        curl_easy_setopt(curl, CURLOPT_READDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                         static_cast<curl_off_t>(file.size()));
        emit logMessage(QStringLiteral("Uploading %1 (%2 bytes)")
                            .arg(config.localPath)
                            .arg(file.size()));
    } else {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit transferCompleted(false,
                "Cannot open local file for writing: " + config.localPath);
            curl_easy_cleanup(curl);
            return;
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        emit logMessage("Downloading to " + config.localPath);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.toUtf8().constData());

    if (!config.username.isEmpty())
        curl_easy_setopt(curl, CURLOPT_USERNAME,
                         config.username.toUtf8().constData());
    if (!config.password.isEmpty())
        curl_easy_setopt(curl, CURLOPT_PASSWORD,
                         config.password.toUtf8().constData());

    if (config.protocol == SCP) {
        if (!config.keyFile.isEmpty())
            curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE,
                             config.keyFile.toUtf8().constData());
        curl_easy_setopt(curl, CURLOPT_SSH_AUTH_TYPES,
                         CURLSSH_AUTH_PASSWORD | CURLSSH_AUTH_PUBLICKEY);
        curl_easy_setopt(curl, CURLOPT_SSH_KEYFUNCTION, sshHostKeyCb);
    }

    if (config.protocol == FTP)
        curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
                         CURLFTP_CREATE_DIR);

    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    emit logMessage("Connecting to " + config.host + "…");

    CURLcode res = curl_easy_perform(curl);
    file.close();

    if (res == CURLE_OK) {
        emit transferCompleted(true, QStringLiteral("%1 transfer completed successfully.").arg(protoName));
    } else if (res == CURLE_ABORTED_BY_CALLBACK) {
        if (config.direction == Download)
            QFile::remove(config.localPath);
        emit transferCompleted(false, "Transfer cancelled by user.");
    } else {
        emit transferCompleted(false,
            QStringLiteral("Transfer failed: %1").arg(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
}

// ---------------------------------------------------------------------------
// TFTP via custom client
// ---------------------------------------------------------------------------
void TransferEngine::transferWithTftp(const TransferConfig &config)
{
    emit logMessage("Starting TFTP transfer…");

    TftpClient tftp;
    connect(&tftp, &TftpClient::progressChanged,
            this, &TransferEngine::progressChanged);
    connect(&tftp, &TftpClient::logMessage,
            this, &TransferEngine::logMessage);

    bool ok = false;
    if (config.direction == Upload)
        ok = tftp.upload(config.host, config.port,
                         config.localPath, config.remotePath, &cancelled_);
    else
        ok = tftp.download(config.host, config.port,
                           config.remotePath, config.localPath, &cancelled_);

    emit transferCompleted(ok,
        ok ? "TFTP transfer completed successfully."
           : "TFTP transfer failed.");
}
