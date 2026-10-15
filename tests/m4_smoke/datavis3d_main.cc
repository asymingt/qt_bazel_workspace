#include <QtCore/QObject>
#include <QtDataVisualization/QBar3DSeries>
#include <QtDataVisualization/QBarDataProxy>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest/QTest>

Q_IMPORT_QML_PLUGIN(QtDataVisualizationPlugin)

class M4DataVis3DSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void testDataVis3D() {
        QBar3DSeries bar3d;
        bar3d.setItemLabelFormat(QStringLiteral("@valueLabel"));
        QCOMPARE(bar3d.itemLabelFormat(), QStringLiteral("@valueLabel"));
    }
};

QTEST_MAIN(M4DataVis3DSmokeTest)
#include "datavis3d_main.moc"
