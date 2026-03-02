#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QThread;
QT_END_NAMESPACE

class TransferEngine;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onProtocolChanged(int index);
    void onBrowseLocal();
    void onBrowseKey();
    void onStartTransfer();
    void onCancelTransfer();
    void onProgressChanged(qint64 transferred, qint64 total);
    void onTransferCompleted(bool success, const QString &message);
    void onLogMessage(const QString &message);

private:
    void setupUi();
    void setupMenus();
    void cleanupTransfer();

    QComboBox      *protocolCombo_ = nullptr;
    QLineEdit      *hostEdit_      = nullptr;
    QSpinBox       *portSpin_      = nullptr;
    QLineEdit      *userEdit_      = nullptr;
    QLineEdit      *passEdit_      = nullptr;
    QLineEdit      *keyEdit_       = nullptr;
    QPushButton    *keyBrowseBtn_  = nullptr;
    QLabel         *keyLabel_      = nullptr;

    QLineEdit      *localEdit_     = nullptr;
    QLineEdit      *remoteEdit_    = nullptr;
    QRadioButton   *uploadRadio_   = nullptr;
    QRadioButton   *downloadRadio_ = nullptr;
    QProgressBar   *progressBar_   = nullptr;
    QLabel         *statusLabel_   = nullptr;
    QPushButton    *startBtn_      = nullptr;
    QPushButton    *cancelBtn_     = nullptr;

    QPlainTextEdit *logEdit_       = nullptr;

    TransferEngine *engine_        = nullptr;
    QThread        *workerThread_  = nullptr;
    bool            transferring_  = false;
};

#endif // MAINWINDOW_H
