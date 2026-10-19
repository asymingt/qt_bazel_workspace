#include <QtCore/QObject>
#include <QtTest/QtTest>

#include "tests/phase4_m2/gl_direct_caller.h"

class GlDirectCallTest : public QObject {
  Q_OBJECT

 private slots:
  void testDirectGlAndGlxCalls() {
    QVERIFY(phase4_m2::verifyDirectGlAndGlxSymbolsLinked());
  }
};

QTEST_MAIN(GlDirectCallTest)
#include "gl_direct_call_test.moc"
