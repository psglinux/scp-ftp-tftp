#include "mainwindow.h"
#include "transferengine.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("GotiKinesis — File Transfer Utility");
    setMinimumSize(620, 540);
    resize(720, 600);

    setupMenus();
    setupUi();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow()
{
    cleanupTransfer();
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------
void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Quit", this, &QWidget::close, QKeySequence::Quit);

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        QMessageBox::about(this, "About GotiKinesis (GK)",
            "<h3>GotiKinesis (GK) v1.0.0</h3>"
            "<p>Cross-platform file transfer utility<br>"
            "supporting <b>SCP</b>, <b>FTP</b>, and <b>TFTP</b>.</p>"
            "<p>Built with C++ / Qt and libcurl.</p>");
    });
}

// ---------------------------------------------------------------------------
// Central widget layout
// ---------------------------------------------------------------------------
void MainWindow::setupUi()
{
    auto *central    = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // ---- Protocol selector ----
    auto *protoLayout = new QHBoxLayout;
    protoLayout->addWidget(new QLabel("Protocol:"));
    protocolCombo_ = new QComboBox;
    protocolCombo_->addItems({
        "SCP  (Secure Copy over SSH)",
        "FTP  (File Transfer Protocol)",
        "TFTP (Trivial File Transfer Protocol)"
    });
    protocolCombo_->setMinimumWidth(260);
    protoLayout->addWidget(protocolCombo_);
    protoLayout->addStretch();
    mainLayout->addLayout(protoLayout);

    // ---- Connection group ----
    auto *connGroup  = new QGroupBox("Connection");
    auto *connLayout = new QFormLayout(connGroup);

    auto *hostRow = new QHBoxLayout;
    hostEdit_ = new QLineEdit;
    hostEdit_->setPlaceholderText("hostname or IP address");
    hostRow->addWidget(hostEdit_, 3);
    hostRow->addWidget(new QLabel("Port:"));
    portSpin_ = new QSpinBox;
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(22);
    hostRow->addWidget(portSpin_, 1);
    connLayout->addRow("Host:", hostRow);

    userEdit_ = new QLineEdit;
    userEdit_->setPlaceholderText("username");
    connLayout->addRow("Username:", userEdit_);

    passEdit_ = new QLineEdit;
    passEdit_->setEchoMode(QLineEdit::Password);
    passEdit_->setPlaceholderText("password");
    connLayout->addRow("Password:", passEdit_);

    auto *keyRow = new QHBoxLayout;
    keyEdit_ = new QLineEdit;
    keyEdit_->setPlaceholderText("path to SSH private key");
    keyBrowseBtn_ = new QPushButton("Browse…");
    keyRow->addWidget(keyEdit_);
    keyRow->addWidget(keyBrowseBtn_);
    keyLabel_ = new QLabel("Key File:");
    connLayout->addRow(keyLabel_, keyRow);

    mainLayout->addWidget(connGroup);

    // ---- Transfer group ----
    auto *xferGroup  = new QGroupBox("Transfer");
    auto *xferLayout = new QFormLayout(xferGroup);

    auto *localRow = new QHBoxLayout;
    localEdit_ = new QLineEdit;
    localEdit_->setPlaceholderText("local file path");
    auto *browseLocalBtn = new QPushButton("Browse…");
    localRow->addWidget(localEdit_);
    localRow->addWidget(browseLocalBtn);
    xferLayout->addRow("Local File:", localRow);

    remoteEdit_ = new QLineEdit;
    remoteEdit_->setPlaceholderText("/remote/path/file.bin");
    xferLayout->addRow("Remote Path:", remoteEdit_);

    auto *dirRow = new QHBoxLayout;
    uploadRadio_   = new QRadioButton("Upload");
    downloadRadio_ = new QRadioButton("Download");
    uploadRadio_->setChecked(true);
    dirRow->addWidget(uploadRadio_);
    dirRow->addWidget(downloadRadio_);
    dirRow->addStretch();
    xferLayout->addRow("Direction:", dirRow);

    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    xferLayout->addRow(progressBar_);

    statusLabel_ = new QLabel;
    xferLayout->addRow(statusLabel_);

    auto *btnRow = new QHBoxLayout;
    startBtn_ = new QPushButton("  Start Transfer  ");
    startBtn_->setMinimumHeight(36);
    cancelBtn_ = new QPushButton("  Cancel  ");
    cancelBtn_->setMinimumHeight(36);
    cancelBtn_->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(startBtn_);
    btnRow->addWidget(cancelBtn_);
    btnRow->addStretch();
    xferLayout->addRow(btnRow);

    mainLayout->addWidget(xferGroup);

    // ---- Log group ----
    auto *logGroup  = new QGroupBox("Log");
    auto *logLayout = new QVBoxLayout(logGroup);
    logEdit_ = new QPlainTextEdit;
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumBlockCount(2000);
    logEdit_->setFont(QFont("Monospace", 9));
    logLayout->addWidget(logEdit_);
    mainLayout->addWidget(logGroup, /*stretch=*/1);

    setCentralWidget(central);

    // ---- Signal / slot wiring ----
    connect(protocolCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProtocolChanged);
    connect(browseLocalBtn, &QPushButton::clicked,
            this, &MainWindow::onBrowseLocal);
    connect(keyBrowseBtn_, &QPushButton::clicked,
            this, &MainWindow::onBrowseKey);
    connect(startBtn_, &QPushButton::clicked,
            this, &MainWindow::onStartTransfer);
    connect(cancelBtn_, &QPushButton::clicked,
            this, &MainWindow::onCancelTransfer);

    onProtocolChanged(0);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void MainWindow::onProtocolChanged(int index)
{
    bool isScp  = (index == 0);
    bool isTftp = (index == 2);

    keyEdit_->setVisible(isScp);
    keyBrowseBtn_->setVisible(isScp);
    keyLabel_->setVisible(isScp);

    userEdit_->setEnabled(!isTftp);
    passEdit_->setEnabled(!isTftp);

    const int defaultPorts[] = {22, 21, 69};
    portSpin_->setValue(defaultPorts[index]);
}

void MainWindow::onBrowseLocal()
{
    QString path = uploadRadio_->isChecked()
        ? QFileDialog::getOpenFileName(this, "Select File to Upload")
        : QFileDialog::getSaveFileName(this, "Save Downloaded File As");
    if (!path.isEmpty())
        localEdit_->setText(path);
}

void MainWindow::onBrowseKey()
{
    QString path = QFileDialog::getOpenFileName(this, "Select SSH Private Key");
    if (!path.isEmpty())
        keyEdit_->setText(path);
}

void MainWindow::onStartTransfer()
{
    if (hostEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Host is required.");
        return;
    }
    if (localEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Local file path is required.");
        return;
    }
    if (remoteEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Remote path is required.");
        return;
    }

    TransferEngine::TransferConfig cfg;
    cfg.protocol   = static_cast<TransferEngine::Protocol>(protocolCombo_->currentIndex());
    cfg.direction  = uploadRadio_->isChecked() ? TransferEngine::Upload
                                               : TransferEngine::Download;
    cfg.host       = hostEdit_->text().trimmed();
    cfg.port       = portSpin_->value();
    cfg.username   = userEdit_->text();
    cfg.password   = passEdit_->text();
    cfg.keyFile    = keyEdit_->text();
    cfg.localPath  = localEdit_->text().trimmed();
    cfg.remotePath = remoteEdit_->text().trimmed();

    cleanupTransfer();

    transferring_ = true;
    startBtn_->setEnabled(false);
    cancelBtn_->setEnabled(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    statusLabel_->clear();
    logEdit_->clear();

    engine_       = new TransferEngine;
    workerThread_ = new QThread;
    engine_->moveToThread(workerThread_);

    connect(engine_, &TransferEngine::progressChanged,
            this, &MainWindow::onProgressChanged, Qt::QueuedConnection);
    connect(engine_, &TransferEngine::transferCompleted,
            this, &MainWindow::onTransferCompleted, Qt::QueuedConnection);
    connect(engine_, &TransferEngine::logMessage,
            this, &MainWindow::onLogMessage, Qt::QueuedConnection);

    connect(workerThread_, &QThread::started, [this, cfg]() {
        engine_->startTransfer(cfg);
    });

    workerThread_->start();
    statusBar()->showMessage("Transferring…");
}

void MainWindow::onCancelTransfer()
{
    if (engine_)
        engine_->cancelTransfer();
    statusBar()->showMessage("Cancelling…");
}

void MainWindow::onProgressChanged(qint64 transferred, qint64 total)
{
    auto fmt = [](qint64 bytes) -> QString {
        if (bytes < 1024)          return QString("%1 B").arg(bytes);
        if (bytes < 1048576)       return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        if (bytes < 1073741824LL)  return QString("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
        return QString("%1 GB").arg(bytes / 1073741824.0, 0, 'f', 2);
    };

    if (total > 0) {
        int pct = static_cast<int>(transferred * 100 / total);
        progressBar_->setRange(0, 100);
        progressBar_->setValue(pct);
        statusLabel_->setText(QString("%1 / %2").arg(fmt(transferred), fmt(total)));
    } else {
        progressBar_->setRange(0, 0);
        statusLabel_->setText(QString("%1 transferred").arg(fmt(transferred)));
    }
}

void MainWindow::onTransferCompleted(bool success, const QString &message)
{
    transferring_ = false;
    startBtn_->setEnabled(true);
    cancelBtn_->setEnabled(false);
    progressBar_->setRange(0, 100);

    if (success) {
        progressBar_->setValue(100);
        statusBar()->showMessage("Transfer completed successfully");
    } else {
        statusBar()->showMessage("Transfer failed");
    }

    onLogMessage(message);
    cleanupTransfer();
}

void MainWindow::onLogMessage(const QString &message)
{
    logEdit_->appendPlainText(message);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void MainWindow::cleanupTransfer()
{
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(5000);
        delete workerThread_;
        workerThread_ = nullptr;
    }
    if (engine_) {
        delete engine_;
        engine_ = nullptr;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (transferring_) {
        auto reply = QMessageBox::question(this, "Transfer in Progress",
            "A file transfer is still running.\nCancel it and quit?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        if (engine_) engine_->cancelTransfer();
        cleanupTransfer();
    }
    event->accept();
}
