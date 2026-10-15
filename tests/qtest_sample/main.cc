#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

class SampleTest : public QObject {
    Q_OBJECT

signals:
    void messageEmitted(const QString &msg);

private slots:
    void testStringOperations() {
        QString str = QStringLiteral("Bazel Qt6");
        QCOMPARE(str.toUpper(), QStringLiteral("BAZEL QT6"));
        QVERIFY(str.startsWith(QStringLiteral("Bazel")));
    }

    void testSignalSpy() {
        QSignalSpy spy(this, &SampleTest::messageEmitted);
        QVERIFY(spy.isValid());
        emit messageEmitted(QStringLiteral("modular"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("modular"));
    }
};

QTEST_MAIN(SampleTest)
#include "main.moc"
