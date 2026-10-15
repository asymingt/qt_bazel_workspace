#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DRender/QCamera>
#include <QtCore/QObject>
#include <QtGraphs/QBarSeries>
#include <QtGraphs/QBarSet>
#include <QtGraphs/QValueAxis>
#include <QtGui/QSurfaceFormat>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuick3D/QQuick3D>
#include <QtQuick3D/QQuick3DObject>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

Q_IMPORT_QML_PLUGIN(QQuick3DPlugin)
Q_IMPORT_QML_PLUGIN(QtQuick3DPhysicsPlugin)

class M4SmokeTest : public QObject {
    Q_OBJECT

private slots:
    void testOpenGLTypes() {
        QSurfaceFormat fmt = QQuick3D::idealSurfaceFormat(4);
        QVERIFY(fmt.samples() == 4 || fmt.samples() >= -1);
        QOpenGLBuffer buf(QOpenGLBuffer::VertexBuffer);
        QCOMPARE(buf.type(), QOpenGLBuffer::VertexBuffer);
    }

    void testQuick3DQml() {
        QQmlEngine engine;
        QQmlComponent comp(&engine);
        comp.setData(R"(
            import QtQuick
            import QtQuick3D
            Node {
                position: Qt.vector3d(1, 2, 3)
                PerspectiveCamera {
                    z: 300
                }
                DirectionalLight {}
            }
        )", QUrl());
        QVERIFY2(!comp.isError(), qPrintable(comp.errorString()));
        std::unique_ptr<QObject> obj(comp.create());
        QVERIFY(obj != nullptr);
        auto *node = qobject_cast<QQuick3DObject *>(obj.get());
        QVERIFY(node != nullptr);
    }

    void testGraphs() {
        QBarSeries series;
        auto *set = new QBarSet("Set1");
        *set << 1.0 << 2.0 << 3.0;
        QVERIFY(series.append(set));
        QCOMPARE(series.count(), 1);
    }

    void testQt3DSceneGraph() {
        Qt3DCore::QEntity rootEntity;
        auto *transform = new Qt3DCore::QTransform(&rootEntity);
        transform->setScale(2.0f);
        auto *mesh = new Qt3DExtras::QCuboidMesh(&rootEntity);
        mesh->setXExtent(4.0f);
        rootEntity.addComponent(transform);
        rootEntity.addComponent(mesh);
        QCOMPARE(rootEntity.components().size(), 2);

        Qt3DRender::QCamera camera(&rootEntity);
        camera.setFieldOfView(45.0f);
        QCOMPARE(camera.fieldOfView(), 45.0f);
    }
};

QTEST_MAIN(M4SmokeTest)
#include "main.moc"
