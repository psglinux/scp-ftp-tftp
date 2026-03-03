#include "transferengine.h"
#include "tftpclient.h"

#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QStringList>
#include <curl/curl.h>

// ---------------------------------------------------------------------------
// libcurl callback context
// ---------------------------------------------------------------------------
struct CurlContext {
    TransferEngine   *engine;
    QFile            *file;
    std::atomic<bool>*cancelled;
    QElapsedTimer    *timer;
    qint64            lastProgressLogBytes = 0;
};

static QString humanSize(qint64 bytes)
{
    if (bytes < 1024)              return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)       return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

// ---------------------------------------------------------------------------
// Curl debug callback -- captures connection, auth, key exchange, errors
// ---------------------------------------------------------------------------
static int curlDebugCb(CURL * /*handle*/, curl_infotype type,
                       char *data, size_t size, void *userptr)
{
    auto *ctx = static_cast<CurlContext *>(userptr);
    QString text = QString::fromUtf8(data, static_cast<int>(size)).trimmed();
    if (text.isEmpty())
        return 0;

    switch (type) {
    case CURLINFO_TEXT:
        emit ctx->engine->logMessage("[curl] " + text);
        break;
    case CURLINFO_HEADER_IN:
        emit ctx->engine->logMessage("[recv] " + text);
        break;
    case CURLINFO_HEADER_OUT:
        emit ctx->engine->logMessage("[send] " + text);
        break;
    case CURLINFO_DATA_IN:
    case CURLINFO_DATA_OUT:
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
        break;
    default:
        break;
    }
    return 0;
}

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

    constexpr qint64 LOG_INTERVAL = 256 * 1024;
    if (now - ctx->lastProgressLogBytes >= LOG_INTERVAL
        || (now > 0 && now == total)) {
        ctx->lastProgressLogBytes = now;
        double elapsed = ctx->timer->elapsed() / 1000.0;
        double speed = (elapsed > 0) ? (now / elapsed) : 0;
        QString pct = (total > 0)
            ? QStringLiteral(" (%1%)").arg(now * 100 / total) : "";
        emit ctx->engine->logMessage(
            QStringLiteral("[progress] %1 / %2%3 @ %4/s")
                .arg(humanSize(now), humanSize(total), pct,
                     humanSize(static_cast<qint64>(speed))));
    }

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

    if (config.direction == Upload && (path.endsWith('/') || path == "/")) {
        QString localName = QFileInfo(config.localPath).fileName();
        if (!localName.isEmpty())
            path += localName;
    }

    return QStringLiteral("%1://%2:%3%4")
               .arg(scheme)
               .arg(config.host)
               .arg(config.port)
               .arg(path);
}

void TransferEngine::transferWithCurl(const TransferConfig &config)
{
    const char *protoName = (config.protocol == SCP) ? "SCP" : "FTP";
    emit logMessage(QStringLiteral("Initializing %1 transfer...").arg(protoName));

    curl_version_info_data *vi = curl_version_info(CURLVERSION_NOW);
    QStringList protos;
    bool hasScp = false;
    for (const char *const *p = vi->protocols; p && *p; ++p) {
        protos << QString::fromUtf8(*p);
        if (qstrcmp(*p, "scp") == 0) hasScp = true;
    }
    emit logMessage(QStringLiteral("[curl] libcurl %1 (protocols: %2)")
                        .arg(vi->version, protos.join(", ")));
    if (config.protocol == SCP && !hasScp) {
        emit transferCompleted(false,
            "libcurl was built without SSH support. SCP transfers are not available. "
            "Supported protocols: " + protos.join(", "));
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        emit transferCompleted(false, "Failed to initialize libcurl.");
        return;
    }

    QFile file(config.localPath);
    QElapsedTimer timer;
    CurlContext ctx{this, &file, &cancelled_, &timer, 0};

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
        emit logMessage(QStringLiteral("Uploading %1 (%2)")
                            .arg(config.localPath, humanSize(file.size())));
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

    if (!config.username.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME,
                         config.username.toUtf8().constData());
        emit logMessage("Auth: username set");
    }
    if (!config.password.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_PASSWORD,
                         config.password.toUtf8().constData());
        emit logMessage("Auth: password set");
    }

    if (config.protocol == SCP) {
        if (!config.keyFile.isEmpty()) {
            QFileInfo keyInfo(config.keyFile);
            if (keyInfo.isDir()) {
                emit logMessage("[warn] SSH key path is a directory, not a file: "
                                + config.keyFile);
                emit logMessage("[warn] Please specify a private key file, e.g. "
                                + config.keyFile + "/id_rsa or id_ed25519");
            } else {
                curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE,
                                 config.keyFile.toUtf8().constData());
                emit logMessage("Auth: SSH key file = " + config.keyFile);
            }
        }
        curl_easy_setopt(curl, CURLOPT_SSH_AUTH_TYPES,
                         CURLSSH_AUTH_PASSWORD | CURLSSH_AUTH_PUBLICKEY);
        curl_easy_setopt(curl, CURLOPT_SSH_KEYFUNCTION, sshHostKeyCb);
        emit logMessage("SSH auth methods: password + publickey");
    }

    if (config.protocol == FTP) {
        curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
                         CURLFTP_CREATE_DIR);
        emit logMessage("FTP: auto-create missing directories enabled");
    }

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curlDebugCb);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &ctx);

    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    emit logMessage("Connecting to " + config.host + ":" +
                    QString::number(config.port) + "...");
    timer.start();

    CURLcode res = curl_easy_perform(curl);
    double elapsed = timer.elapsed() / 1000.0;
    file.close();

    if (res == CURLE_OK) {
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        double totalTime = 0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
        curl_off_t totalBytes = 0;
        if (config.direction == Upload)
            curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &totalBytes);
        else
            curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &totalBytes);

        double avgSpeed = (elapsed > 0) ? (totalBytes / elapsed) : 0;
        emit logMessage(QStringLiteral("[summary] %1 complete: %2 in %3s (%4/s)")
                            .arg(protoName, humanSize(totalBytes))
                            .arg(elapsed, 0, 'f', 2)
                            .arg(humanSize(static_cast<qint64>(avgSpeed))));
        emit transferCompleted(true,
            QStringLiteral("%1 transfer completed successfully.").arg(protoName));
    } else if (res == CURLE_ABORTED_BY_CALLBACK) {
        if (config.direction == Download)
            QFile::remove(config.localPath);
        emit logMessage("[cancelled] Transfer aborted by user.");
        emit transferCompleted(false, "Transfer cancelled by user.");
    } else {
        emit logMessage(QStringLiteral("[error] curl code %1: %2")
                            .arg(static_cast<int>(res))
                            .arg(curl_easy_strerror(res)));
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
