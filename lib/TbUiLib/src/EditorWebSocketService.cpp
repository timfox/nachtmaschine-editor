/*
 Nachtmaschine — optional localhost WebSocket server for editor automation / tooling.
*/

#include "ui/EditorWebSocketService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>

namespace tb::ui
{

EditorWebSocketService::EditorWebSocketService(const quint16 port, QObject* parent)
  : QObject{parent}
  , m_server{QStringLiteral("TrenchBroomEditor"), QWebSocketServer::NonSecureMode, this}
{
  if (!m_server.listen(QHostAddress::LocalHost, port))
  {
    qWarning(
      "EditorWebSocketService: could not listen on 127.0.0.1:%u (%s)",
      static_cast<unsigned>(port),
      qPrintable(m_server.errorString()));
    return;
  }

  connect(
    &m_server, &QWebSocketServer::newConnection, this, &EditorWebSocketService::onNewConnection);
}

void EditorWebSocketService::onNewConnection()
{
  while (m_server.hasPendingConnections())
  {
    auto* socket = m_server.nextPendingConnection();
    if (socket == nullptr)
    {
      continue;
    }
    socket->setParent(this);

    const QJsonObject hello{
      {QStringLiteral("type"), QStringLiteral("hello")},
      {QStringLiteral("editor"), QStringLiteral("TrenchBroom")},
      {QStringLiteral("api"), 1},
      {QStringLiteral("port"), static_cast<int>(m_server.serverPort())},
    };
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument{hello}.toJson(QJsonDocument::Compact)));

    QObject::connect(socket, &QWebSocket::textMessageReceived, socket, [socket](const QString& text) {
      QJsonParseError parseErr{};
      const auto doc = QJsonDocument::fromJson(text.toUtf8(), &parseErr);
      if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
      {
        return;
      }
      const auto obj = doc.object();
      if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("echo"))
      {
        return;
      }
      QJsonObject reply{
        {QStringLiteral("type"), QStringLiteral("echo_reply")},
        {QStringLiteral("payload"), obj.value(QStringLiteral("payload"))},
      };
      socket->sendTextMessage(QString::fromUtf8(QJsonDocument{reply}.toJson(QJsonDocument::Compact)));
    });
  }
}

} // namespace tb::ui
