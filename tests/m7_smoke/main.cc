#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTranslator>
#include <QtGrpc/QGrpcChannelOptions>
#include <QtGrpc/QGrpcHttp2Channel>
#include <QtHelp/QHelpEngineCore>
#include <QtProtobuf/QProtobufJsonSerializer>
#include <QtProtobuf/QProtobufSerializer>
#include <QtUiTools/QUiLoader>
#include <QtWebView/QtWebView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

#include "rules_cc/cc/runfiles/runfiles.h"

#include "test.qpb.h"
#include "test_client.grpc.qpb.h"

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // 1. @qtwebview: initialize QtWebView
  QtWebView::initialize();

  // 2. @qtgrpc: round-trip protobuf serialization + instantiate generated gRPC client
  m7smoke::EchoRequest req;
  req.setMessage(QStringLiteral("Hello QtProtobuf"));
  req.setCount(42);

  QProtobufSerializer serializer;
  const QByteArray bytes = req.serialize(&serializer);
  assert(!bytes.isEmpty());

  m7smoke::EchoRequest decoded;
  const bool ok = decoded.deserialize(&serializer, bytes);
  assert(ok);
  assert(decoded.message() == QStringLiteral("Hello QtProtobuf"));
  assert(decoded.count() == 42);

  QProtobufJsonSerializer json_serializer;
  const QByteArray json_bytes = req.serialize(&json_serializer);
  assert(!json_bytes.isEmpty());

  m7smoke::EchoService::Client grpc_client;
  auto channel = std::make_shared<QGrpcHttp2Channel>(
      QUrl(QStringLiteral("http://localhost:50051")));
  grpc_client.attachChannel(channel);

  // 3. @qttools (:uitools): load a .ui form in memory with QUiLoader
  const QByteArray ui_xml =
      "<ui version=\"4.0\">"
      " <class>SmokeWidget</class>"
      " <widget class=\"QWidget\" name=\"SmokeWidget\">"
      "  <property name=\"objectName\">"
      "   <string>SmokeWidgetInstance</string>"
      "  </property>"
      " </widget>"
      "</ui>";
  QBuffer ui_buf;
  ui_buf.setData(ui_xml);
  assert(ui_buf.open(QIODevice::ReadOnly));

  QUiLoader loader;
  std::unique_ptr<QWidget> widget(loader.load(&ui_buf));
  assert(widget != nullptr);
  assert(widget->objectName() == QStringLiteral("SmokeWidgetInstance"));

  // 4. @qttools (:help): initialize QHelpEngineCore on a temporary collection file
  QTemporaryDir temp_dir;
  assert(temp_dir.isValid());
  QHelpEngineCore help_engine(temp_dir.filePath(QStringLiteral("smoke.qhc")));
  assert(help_engine.setupData());

  // 5. @qttranslations: find and load a compiled .qm catalog. Resolve it via
  // the runfiles library rather than scanning the cwd: on Windows Bazel does
  // not materialize a runfiles tree (manifest only), so relative lookups fail.
  std::string rf_error;
  std::unique_ptr<rules_cc::cc::runfiles::Runfiles> runfiles(
      rules_cc::cc::runfiles::Runfiles::CreateForTest(&rf_error));
  assert(runfiles != nullptr);
  const std::string qm_path =
      runfiles->Rlocation("qttranslations+/translations_qm/qtbase_de.qm");
  assert(!qm_path.empty());
  QTranslator translator;
  const bool loaded_qm = translator.load(QString::fromStdString(qm_path));
  assert(loaded_qm);

  std::printf("Milestone 7 smoke test passed!\n");
  return 0;
}
