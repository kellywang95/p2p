#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QMutex>
#include <QTimer>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket(QObject *parent);
	~NetSocket();

    	// Bind this socket to a P2Papp-specific default port.
    	bool bind();
	// bool readUdp(QVariantMap *map);
	int genRandNum();
	int getWritePort();
	void sendUdpDatagram(const QVariantMap &qMap, int port);
	void sendUdpDatagram(const QMap<QString, QVariantMap> &qMap, int port);
	
	
	int myPort;
	int sendPort;
	int recvPort;
	QHostAddress HostAddress;
	QString originName;

private:
	int myPortMin, myPortMax;
};



class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
	void gotReadyRead();
	void antiEntropyHandler();

private:
	QTextEdit *textview;
	QLineEdit *textline;
	NetSocket *udpSocket;
	QTimer *antiEntropyTimer;
	QMap<QString, QMap<quint32, QString> > allMessages;
	QVariantMap myWants;
	QMutex mutex1;  // for myWants
	QMutex mutex2;  // for allMessages

	void writeRumorMessage(QString &origin, quint32 seqNo, QString &text, quint16 port, bool addToMsg);
	void writeStatusMessage(int port);
	void addToMessages(QVariantMap &qMap);
	void handleStatusMsg(QVariantMap &gotWants, quint16 port);
	void handleRumorMsg(QVariantMap &rumorMap);
};


#endif // P2PAPP_MAIN_HH
