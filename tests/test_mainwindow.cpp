#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QMenuBar>
#include "mainwindow.h"

class TestMainWindow : public QObject {
    Q_OBJECT

private:
    template <typename T>
    T *find(QWidget *parent, const QString &objectName = {})
    {
        if (!objectName.isEmpty())
            return parent->findChild<T *>(objectName);
        auto list = parent->findChildren<T *>();
        return list.isEmpty() ? nullptr : list.first();
    }

    template <typename T>
    QList<T *> findAll(QWidget *parent)
    {
        return parent->findChildren<T *>();
    }

private slots:

    // ---- Window creation ----

    void windowCreated()
    {
        MainWindow w;
        QVERIFY(w.windowTitle().contains("GotiKinesis"));
    }

    void minimumSizeSet()
    {
        MainWindow w;
        QVERIFY(w.minimumWidth()  >= 600);
        QVERIFY(w.minimumHeight() >= 500);
    }

    void centralWidgetExists()
    {
        MainWindow w;
        QVERIFY(w.centralWidget() != nullptr);
    }

    void menuBarExists()
    {
        MainWindow w;
        QVERIFY(w.menuBar() != nullptr);
        QVERIFY(w.menuBar()->actions().size() >= 2);
    }

    void statusBarShowsReady()
    {
        MainWindow w;
        QCOMPARE(w.statusBar()->currentMessage(), QString("Ready"));
    }

    // ---- Widget existence ----

    void protocolComboExists()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 3);
    }

    void hostEditExists()
    {
        MainWindow w;
        auto edits = findAll<QLineEdit>(&w);
        QVERIFY(edits.size() >= 4);
    }

    void portSpinExists()
    {
        MainWindow w;
        auto *spin = find<QSpinBox>(&w);
        QVERIFY(spin != nullptr);
        QVERIFY(spin->minimum() >= 1);
        QVERIFY(spin->maximum() <= 65535);
    }

    void radioButtonsExist()
    {
        MainWindow w;
        auto radios = findAll<QRadioButton>(&w);
        QCOMPARE(radios.size(), 2);
    }

    void progressBarExists()
    {
        MainWindow w;
        auto *bar = find<QProgressBar>(&w);
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->value(), 0);
    }

    void logEditExists()
    {
        MainWindow w;
        auto *log = find<QPlainTextEdit>(&w);
        QVERIFY(log != nullptr);
        QVERIFY(log->isReadOnly());
    }

    void buttonsExist()
    {
        MainWindow w;
        auto btns = findAll<QPushButton>(&w);
        QVERIFY(btns.size() >= 4);
    }

    // ---- Initial state for SCP ----

    void initialProtocolIsScp()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(combo->currentText().contains("SCP"));
    }

    void initialPortIs22()
    {
        MainWindow w;
        auto *spin = find<QSpinBox>(&w);
        QCOMPARE(spin->value(), 22);
    }

    void uploadSelectedByDefault()
    {
        MainWindow w;
        auto radios = findAll<QRadioButton>(&w);
        QRadioButton *upload = nullptr;
        for (auto *r : radios) {
            if (r->text() == "Upload")
                upload = r;
        }
        QVERIFY(upload != nullptr);
        QVERIFY(upload->isChecked());
    }

    void startButtonEnabled()
    {
        MainWindow w;
        auto btns = findAll<QPushButton>(&w);
        QPushButton *start = nullptr;
        for (auto *b : btns) {
            if (b->text().contains("Start"))
                start = b;
        }
        QVERIFY(start != nullptr);
        QVERIFY(start->isEnabled());
    }

    void cancelButtonDisabled()
    {
        MainWindow w;
        auto btns = findAll<QPushButton>(&w);
        QPushButton *cancel = nullptr;
        for (auto *b : btns) {
            if (b->text().contains("Cancel"))
                cancel = b;
        }
        QVERIFY(cancel != nullptr);
        QVERIFY(!cancel->isEnabled());
    }

    // ---- Protocol switching ----

    void switchToFtpChangesPort()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        auto *spin  = find<QSpinBox>(&w);

        combo->setCurrentIndex(1);
        QCOMPARE(spin->value(), 21);
    }

    void switchToTftpChangesPort()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        auto *spin  = find<QSpinBox>(&w);

        combo->setCurrentIndex(2);
        QCOMPARE(spin->value(), 69);
    }

    void switchBackToScpRestoresPort()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        auto *spin  = find<QSpinBox>(&w);

        combo->setCurrentIndex(2);
        QCOMPARE(spin->value(), 69);

        combo->setCurrentIndex(0);
        QCOMPARE(spin->value(), 22);
    }

    void tftpDisablesUserPass()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        auto edits  = findAll<QLineEdit>(&w);

        combo->setCurrentIndex(2);

        QLineEdit *userEdit = nullptr;
        QLineEdit *passEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("username"))
                userEdit = e;
            else if (e->placeholderText().contains("password"))
                passEdit = e;
        }
        QVERIFY(userEdit != nullptr);
        QVERIFY(passEdit != nullptr);
        QVERIFY(!userEdit->isEnabled());
        QVERIFY(!passEdit->isEnabled());
    }

    void ftpEnablesUserPass()
    {
        MainWindow w;
        auto *combo = find<QComboBox>(&w);
        auto edits  = findAll<QLineEdit>(&w);

        combo->setCurrentIndex(2);
        combo->setCurrentIndex(1);

        QLineEdit *userEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("username"))
                userEdit = e;
        }
        QVERIFY(userEdit != nullptr);
        QVERIFY(userEdit->isEnabled());
    }

    void scpShowsKeyField()
    {
        MainWindow w;
        w.show();
        auto *combo = find<QComboBox>(&w);
        auto edits  = findAll<QLineEdit>(&w);

        combo->setCurrentIndex(0);

        QLineEdit *keyEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("private key"))
                keyEdit = e;
        }
        QVERIFY(keyEdit != nullptr);
        QVERIFY(keyEdit->isVisible());
    }

    void ftpHidesKeyField()
    {
        MainWindow w;
        w.show();
        auto *combo = find<QComboBox>(&w);
        auto edits  = findAll<QLineEdit>(&w);

        combo->setCurrentIndex(1);

        QLineEdit *keyEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("private key"))
                keyEdit = e;
        }
        QVERIFY(keyEdit != nullptr);
        QVERIFY(!keyEdit->isVisible());
    }

    void tftpHidesKeyField()
    {
        MainWindow w;
        w.show();
        auto *combo = find<QComboBox>(&w);
        auto edits  = findAll<QLineEdit>(&w);

        combo->setCurrentIndex(2);

        QLineEdit *keyEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("private key"))
                keyEdit = e;
        }
        QVERIFY(keyEdit != nullptr);
        QVERIFY(!keyEdit->isVisible());
    }

    // ---- Password field ----

    void passwordFieldIsObscured()
    {
        MainWindow w;
        auto edits = findAll<QLineEdit>(&w);

        QLineEdit *passEdit = nullptr;
        for (auto *e : edits) {
            if (e->placeholderText().contains("password"))
                passEdit = e;
        }
        QVERIFY(passEdit != nullptr);
        QCOMPARE(passEdit->echoMode(), QLineEdit::Password);
    }

    // ---- Log area ----

    void logIsReadOnly()
    {
        MainWindow w;
        auto *log = find<QPlainTextEdit>(&w);
        QVERIFY(log->isReadOnly());
    }

    // ---- Progress bar initial state ----

    void progressBarStartsAtZero()
    {
        MainWindow w;
        auto *bar = find<QProgressBar>(&w);
        QCOMPARE(bar->value(), 0);
        QCOMPARE(bar->minimum(), 0);
        QCOMPARE(bar->maximum(), 100);
    }

    // ---- Window destruction is clean ----

    void destroyWithoutCrash()
    {
        auto *w = new MainWindow;
        w->show();
        delete w;
        QVERIFY(true);
    }

    void multipleInstancesNoConflict()
    {
        MainWindow w1;
        MainWindow w2;
        auto *combo1 = find<QComboBox>(&w1);
        auto *combo2 = find<QComboBox>(&w2);

        combo1->setCurrentIndex(1);
        combo2->setCurrentIndex(2);

        auto *spin1 = find<QSpinBox>(&w1);
        auto *spin2 = find<QSpinBox>(&w2);

        QCOMPARE(spin1->value(), 21);
        QCOMPARE(spin2->value(), 69);
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
