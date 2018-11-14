
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);
	
	// Create a UDP network socket
	udpSocket = new NetSocket(this);
	if (!udpSocket->bind())
		exit(1);

	myWants[udpSocket->originName] = 0;

	/*
	mTimeoutTimer = new QTimer(this);
	connect(myTimeoutTimer, SIGNAL(timeout()),
            this, SLOT(timeoutHandler())); 
	*/
	antiEntropyTimer = new QTimer(this);
	antiEntropyTimer->start(10 * 1000);
	connect(antiEntropyTimer, SIGNAL(timeout()),
            this, SLOT(antiEntropyHandler()));

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
	// Register a callback on the textline's readyRead signal
	// so that we can send the message entered by the user.
	connect(udpSocket, SIGNAL(readyRead()),
		this, SLOT(gotReadyRead()));
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	QString origin = udpSocket->originName;
	qDebug() <<"origin:"<< origin;
	QString message = textline->text();
	quint32 seqNo = myWants[origin].toInt();

	qDebug() <<"seqNo:" << seqNo;
	writeRumorMessage(origin, seqNo, message, -1, true);
	
	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::gotReadyRead() {
	QVariantMap qMap;
	QMap<QString, QVariantMap> statusMap;
	QHostAddress serverAdd;
	quint16 serverPort;

RECV:
	QByteArray mapData(udpSocket->pendingDatagramSize(), Qt::Uninitialized);
	udpSocket->readDatagram(mapData.data(), mapData.size(), &serverAdd, &serverPort);
	QDataStream inStream(&mapData, QIODevice::ReadOnly);
	udpSocket->recvPort = serverPort;
	inStream >> (qMap);
	if (qMap.contains("Want"))
	{
	    QDataStream wantStream(&mapData, QIODevice::ReadOnly);
	    wantStream >> statusMap;
	    qDebug() << "Receive status msg: " << statusMap;
	    handleStatusMsg(statusMap["Want"], udpSocket->recvPort);
	}
	else if(qMap.contains("ChatText"))
	{
	    qDebug() << "Receive rumor msg: " << qMap;
	    handleRumorMsg(qMap);
	}
	else
	{
	    qDebug() << "Receive unrecognized msg" << qMap;
	}
	// mTimeoutTimer->stop();
    	if (udpSocket->hasPendingDatagrams()) {
		goto RECV;
	}
}


// if timer fires, send out status message
void ChatDialog::antiEntropyHandler() {
    qDebug() << "AntiEntropyHandler called";
    writeStatusMessage(udpSocket->getWritePort());
    antiEntropyTimer->start(10 * 1000);
}


void ChatDialog::handleRumorMsg(QVariantMap &rumorMap) {
	qDebug() << "handle rumor: " << rumorMap;
	QString text = rumorMap["ChatText"].toString();
	QString origin = rumorMap["Origin"].toString();
	quint32 seqNo = rumorMap["SeqNo"].toInt();

	// Process msg from other hosts
	if (origin != udpSocket->originName) {

		if (!myWants.contains(origin)) {
			// new host appear
			mutex1.lock();
		    	myWants[origin] = 0;
			mutex1.unlock();
		}
		if (seqNo == (quint32) myWants[origin].toInt()) {
			writeRumorMessage(origin, seqNo, text, -1, true);
		}
		writeStatusMessage(udpSocket->recvPort);
	}
}



void ChatDialog::writeRumorMessage(QString &origin, quint32 seqNo, QString &text, quint16 port, bool addToMsg)
{
	// Gossip message
	QVariantMap qMap;
	qMap["ChatText"] = text;
	qMap["Origin"] = origin;
	qMap["SeqNo"] = seqNo;
	qDebug() << "Write message" << text;
	if (port == (quint16) -1) {
		port = udpSocket->getWritePort();
	}
	mutex2.lock();
	if (addToMsg) addToMessages(qMap);
	mutex2.unlock();
	udpSocket->sendUdpDatagram(qMap, port);
	if ((quint32) myWants[origin].toInt() == seqNo) {
		mutex1.lock();
		myWants[origin] = myWants[origin].toInt() + 1;
		mutex1.unlock();
	}
}

void ChatDialog::handleStatusMsg(QVariantMap &gotWants, quint16 port) {
	qDebug() << "handle wants: " << gotWants << "\nfrom port : " << port;
	for(QVariantMap::const_iterator iter = gotWants.begin(); iter != gotWants.end(); ++iter) {
		qDebug() << iter.key() << iter.value();
		if (!myWants.contains(iter.key())) {
			myWants[iter.key()] = 0;
		}
		if (myWants[iter.key()].toInt() < iter.value().toInt()) {
			// Send Status back
			writeStatusMessage(port);
			return;
		} else if (myWants[iter.key()].toInt() > iter.value().toInt()) {
			// Send rumor back
			QString origin = udpSocket->originName;
			QString message = allMessages[iter.key()][iter.value().toInt()];
			quint32 seqNo = iter.value().toInt();
			writeRumorMessage(origin, seqNo, message, port, false);
			return;
		}
	}
}

void ChatDialog::writeStatusMessage(int port)
{
	QMap<QString, QVariantMap> statusMap;
	statusMap["Want"] = myWants;
	qDebug() << "Sending Status: " << statusMap;
	udpSocket->sendUdpDatagram(statusMap, port);
}


void ChatDialog::addToMessages(QVariantMap &qMap)
{
	QString message = qMap["ChatText"].toString();
	QString origin = qMap["Origin"].toString();
	quint32 seqNo = qMap["SeqNo"].toInt();
	
	if (message.isEmpty()) return;
	if (!allMessages.contains(origin)){
		// first message from origin
		QMap<quint32, QString> tmpMap;
        	tmpMap.insert(seqNo, message);
		allMessages.insert(origin, tmpMap);
	} else {
		if (!allMessages[origin].contains(seqNo)) {
			allMessages[origin].insert(seqNo, message);
		}
	}
	this->textview->append(origin + ">: " + message);
}

NetSocket::NetSocket(QObject *parent = NULL): QUdpSocket(parent)
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
	// get host address
	HostAddress = QHostAddress(QHostAddress::LocalHost);
    	qDebug() << HostAddress.toString();
    	QHostInfo info;
    	originName = info.localHostName() + "-" + QString::number(genRandNum());
   	qDebug() << originName;
}


int NetSocket::genRandNum()
{
    QDateTime current = QDateTime::currentDateTime();
    uint msecs = current.toTime_t();
    qsrand(msecs);
    return qrand();
}

NetSocket::~NetSocket() {}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			myPort = p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}


int NetSocket::getWritePort()
{
	// Determine which port to send to
	sendPort = myPort == myPortMin ? myPort + 1 :myPort == myPortMax ? myPort - 1 :(genRandNum() % 2) == 0 ? myPort + 1: myPort - 1;
    	qDebug() << "Send to Port: " << QString::number(sendPort);
   	return sendPort;
}


void NetSocket::sendUdpDatagram(const QVariantMap &qMap, int port)
{
	if (qMap.isEmpty()) return;

	QByteArray mapData;
	QDataStream outStream(&mapData, QIODevice::WriteOnly);
	outStream << qMap;
	qDebug() << "sending " << qMap <<  " via UDP to port" << port;
	this->writeDatagram(mapData, HostAddress, port);
}

void NetSocket::sendUdpDatagram(const QMap<QString, QVariantMap> &qMap, int port)
{
	if (qMap.isEmpty()) return;

	QByteArray mapData;
	QDataStream outStream(&mapData, QIODevice::WriteOnly);
	outStream << qMap;
	qDebug() << "sending " << qMap <<  " via UDP to port" << port;
	this->writeDatagram(mapData, HostAddress, port);
}


int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

