#include <QApplication>
#include <QCommandLineParser>
#include <cstring>
#include <iostream>

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "mainwindow.h"
#include "transferengine.h"

#ifdef _WIN32
static void attachConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();
    FILE *dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$",  "r", stdin);
}
#endif

static bool needsConsole(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cli")     == 0 ||
            std::strcmp(argv[i], "--help")    == 0 ||
            std::strcmp(argv[i], "-h")        == 0 ||
            std::strcmp(argv[i], "--version") == 0 ||
            std::strcmp(argv[i], "-v")        == 0)
            return true;
    }
    return false;
}

static int runCli(const QCommandLineParser &parser)
{
    QString protocol = parser.value("protocol").toLower();
    QString host     = parser.value("host");
    QString local    = parser.value("local");
    QString remote   = parser.value("remote");

    if (protocol.isEmpty() || host.isEmpty() || local.isEmpty() || remote.isEmpty()) {
        std::cerr << "Error: --protocol, --host, --local, and --remote are required in CLI mode\n";
        return 1;
    }

    TransferEngine::TransferConfig cfg;

    if (protocol == "scp")       cfg.protocol = TransferEngine::SCP;
    else if (protocol == "ftp")  cfg.protocol = TransferEngine::FTP;
    else if (protocol == "tftp") cfg.protocol = TransferEngine::TFTP;
    else {
        std::cerr << "Error: protocol must be scp, ftp, or tftp\n";
        return 1;
    }

    cfg.direction = parser.isSet("download") ? TransferEngine::Download
                                             : TransferEngine::Upload;
    cfg.host      = host;
    cfg.port      = parser.value("port").toInt();
    if (cfg.port == 0) {
        const int defaults[] = {22, 21, 69};
        cfg.port = defaults[static_cast<int>(cfg.protocol)];
    }
    cfg.username   = parser.value("user");
    cfg.password   = parser.value("password");
    cfg.keyFile    = parser.value("key");
    cfg.localPath  = local;
    cfg.remotePath = remote;

    TransferEngine engine;
    bool success = false;

    QObject::connect(&engine, &TransferEngine::logMessage, [](const QString &msg) {
        std::cout << msg.toStdString() << std::endl;
    });
    QObject::connect(&engine, &TransferEngine::progressChanged,
                     [](qint64 transferred, qint64 total) {
        if (total > 0) {
            int pct = static_cast<int>(transferred * 100 / total);
            std::cout << "\rProgress: " << pct << "% ("
                      << transferred << " / " << total << " bytes)" << std::flush;
        }
    });
    QObject::connect(&engine, &TransferEngine::transferCompleted,
                     [&](bool ok, const QString &msg) {
        std::cout << "\n" << msg.toStdString() << std::endl;
        success = ok;
    });

    engine.startTransfer(cfg);
    return success ? 0 : 1;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    if (needsConsole(argc, argv))
        attachConsole();
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    QApplication app(argc, argv);
    app.setApplicationName("GotiKinesis");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("GotiKinesis");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Cross-platform SCP / FTP / TFTP file transfer utility");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption({"cli",      "Run in command-line mode (no GUI)"});
    parser.addOption({"protocol", "Protocol: scp, ftp, or tftp",     "protocol"});
    parser.addOption({"host",     "Remote hostname or IP address",   "host"});
    parser.addOption({"port",     "Remote port number",              "port"});
    parser.addOption({"user",     "Username for authentication",     "user"});
    parser.addOption({"password", "Password for authentication",     "password"});
    parser.addOption({"key",      "SSH private key file (SCP only)", "keyfile"});
    parser.addOption({"upload",   "Upload local file to remote"});
    parser.addOption({"download", "Download remote file to local"});
    parser.addOption({"local",    "Local file path",                 "path"});
    parser.addOption({"remote",   "Remote file path",                "path"});

    parser.process(app);

    int rc = 0;
    if (parser.isSet("cli")) {
        rc = runCli(parser);
    } else {
        MainWindow w;
        w.show();
        rc = app.exec();
    }

    curl_global_cleanup();
    return rc;
}
