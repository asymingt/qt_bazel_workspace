// Smoke test for @qtbase//:network, :sql (+ :qsqlite) and :xml.
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtXml/QDomDocument>

#include <cstdio>

#define CHECK(cond, msg)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL: %s\n", msg);                             \
      return 1;                                                            \
    }                                                                      \
    std::fprintf(stderr, "ok: %s\n", msg);                                 \
  } while (0)

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);

  // --- QtNetwork: OpenSSL backend linked and loadable.
  CHECK(QSslSocket::supportsSsl(), "QSslSocket::supportsSsl()");
  qInfo() << "TLS backend:" << QSslSocket::activeBackend()
          << "lib:" << QSslSocket::sslLibraryVersionString();

  // --- QtNetwork: loopback TCP round trip.
  QTcpServer server;
  CHECK(server.listen(QHostAddress::LocalHost, 0), "QTcpServer::listen");
  QTcpSocket client;
  client.connectToHost(QHostAddress::LocalHost, server.serverPort());
  CHECK(client.waitForConnected(5000), "client connected");
  CHECK(server.waitForNewConnection(5000), "server accepted");
  QTcpSocket *peer = server.nextPendingConnection();
  CHECK(peer != nullptr, "nextPendingConnection");
  client.write("ping");
  CHECK(client.waitForBytesWritten(5000), "bytes written");
  CHECK(peer->waitForReadyRead(5000), "peer ready read");
  CHECK(peer->readAll() == "ping", "payload round trip");

  // --- QtSql: static QSQLITE driver via Q_IMPORT_PLUGIN.
  CHECK(QSqlDatabase::drivers().contains("QSQLITE"), "QSQLITE driver listed");
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    CHECK(db.open(), "sqlite :memory: open");
    QSqlQuery q(db);
    CHECK(q.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"),
          "create table");
    CHECK(q.exec("INSERT INTO t (name) VALUES ('qt'), ('bazel')"), "insert");
    CHECK(q.exec("SELECT COUNT(*) FROM t") && q.next() && q.value(0).toInt() == 2,
          "select count == 2");
  }

  // --- QtXml: DOM parse.
  QDomDocument doc;
  CHECK(doc.setContent(QByteArray("<root><a x=\"1\"/></root>")), "dom parse");
  CHECK(doc.documentElement().firstChildElement("a").attribute("x") == "1",
        "dom attribute lookup");

  std::fprintf(stderr, "PASS\n");
  return 0;
}
