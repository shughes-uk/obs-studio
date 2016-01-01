
#ifndef ECHOSERVER_H
#define ECHOSERVER_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class EchoServer : public QObject
{
	Q_OBJECT
public:
	explicit EchoServer(quint16 port, bool debug = false, QObject *parent = Q_NULLPTR);
	~EchoServer();

signals:
	void closed();
	void startStreamRequested();
public slots:
	void sendStreamStarted();

private slots:
	void onNewConnection();
	void processTextMessage(QString message);
	void processBinaryMessage(QByteArray message);
	void socketDisconnected();

private:
	void processJson();
	QWebSocketServer *m_pWebSocketServer;
	QList<QWebSocket *> m_clients;
	bool m_debug;
};

#endif //ECHOSERVER_H