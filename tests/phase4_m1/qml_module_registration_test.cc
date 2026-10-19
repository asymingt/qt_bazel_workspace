#include <memory>

#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>

class QmlModuleRegistrationTest : public QObject {
  Q_OBJECT

 private:
  void verifyQmlSnippet(const char* name, const QByteArray& qmlCode) {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(qmlCode, QUrl());
    QVERIFY2(!component.isError(),
             qPrintable(QStringLiteral("%1 failed to compile: %2")
                            .arg(QString::fromUtf8(name),
                                 component.errorString())));
    std::unique_ptr<QObject> obj(component.create());
    QVERIFY2(obj != nullptr,
             qPrintable(QStringLiteral("%1 failed to instantiate: %2")
                            .arg(QString::fromUtf8(name),
                                 component.errorString())));
  }

 private slots:
  void testQtLocationMapView() {
    // MapView is a QML-file component in QtLocation (MapView.qml).
    verifyQmlSnippet("QtLocation.MapView", R"(
      import QtQuick
      import QtLocation
      MapView {}
    )");
  }

  void testVirtualKeyboardQmlTypes() {
    // EnterKey is a QML-file component in QtQuick.VirtualKeyboard, and
    // InputEngine / ShiftHandler / VirtualKeyboardSettings come from
    // re-exported submodules.
    verifyQmlSnippet("QtQuick.VirtualKeyboard.EnterKey", R"(
      import QtQuick
      import QtQuick.VirtualKeyboard
      EnterKey {}
    )");
    // Also verify PinyinInputMethod is reachable via QtQuick.VirtualKeyboard.Plugins
    verifyQmlSnippet("QtQuick.VirtualKeyboard.Plugins", R"(
      import QtQuick
      import QtQuick.VirtualKeyboard.Plugins
      PinyinInputMethod {}
    )");
  }

  void testQt3DQmlTypes() {
    verifyQmlSnippet("Qt3D.Core+Render+Extras+Input+Logic+Animation+Scene3D", R"(
      import QtQuick
      import Qt3D.Core
      import Qt3D.Render
      import Qt3D.Input
      import Qt3D.Logic
      import Qt3D.Animation
      import Qt3D.Extras
      import QtQuick.Scene3D
      Entity {
        Transform { scale: 2.0 }
        Camera { fieldOfView: 45.0 }
        DirectionalLight {}
        PhongMaterial {}
        CuboidMesh {}
        InputSettings {}
        FrameAction {}
      }
    )");
  }

  void testQtQuick3DPhysicsTypes() {
    verifyQmlSnippet("QtQuick3D.Physics+Helpers", R"(
      import QtQuick
      import QtQuick3D
      import QtQuick3D.Physics
      import QtQuick3D.Physics.Helpers
      Node {
        PhysicsWorld {}
        StaticRigidBody {
          collisionShapes: BoxShape {}
        }
        CapsuleGeometry {}
      }
    )");
  }

  void testQtGraphsTypes() {
    verifyQmlSnippet("QtGraphs", R"(
      import QtQuick
      import QtGraphs
      Bar3DSeries {}
    )");
  }

  void testQtDataVisualizationTypes() {
    verifyQmlSnippet("QtDataVisualization", R"(
      import QtQuick
      import QtDataVisualization
      Bar3DSeries {}
    )");
  }
};

QTEST_MAIN(QmlModuleRegistrationTest)
#include "qml_module_registration_test.moc"
