#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	int myPort;
	int sendPort;
	QHostAddress HostAddress;

	NetSocket(QObject *parent);
	QString originName;
    	// Bind this socket to a P2Papp-specific default port.
    	bool bind();
	//void writeUdp(const QVariantMap &map, int index);
	// bool readUdp(QVariantMap *map);
	int genRandNum();
	int getReceiverPort();
	~NetSocket();
	
	

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

private:
	QTextEdit *textview;
	QLineEdit *textline;
	NetSocket *udpSocket;
	QMap<QString, quint32> myWants;
	void writeRumorMessage(QString &origin, quint32 seqNo, QString &text);
	
};


#endif // P2PAPP_MAIN_HH
