/*
 Nachtmaschine — optional localhost WebSocket server for editor automation / tooling.
*/

#pragma once

#include <QObject>
#include <QWebSocketServer>

namespace tb::ui
{

/**
 * Listens on 127.0.0.1 and accepts WebSocket connections. On connect, sends a small JSON
 * hello. Incoming text messages that parse as JSON objects with "type":"echo" are echoed
 * back with "type":"echo_reply".
 */
class EditorWebSocketService final : public QObject
{
  Q_OBJECT

public:
  explicit EditorWebSocketService(quint16 port, QObject* parent = nullptr);

private slots:
  void onNewConnection();

private:
  QWebSocketServer m_server;
};

} // namespace tb::ui
