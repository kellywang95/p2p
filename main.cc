
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

	udpSocket->changeRandomPort();
	udpSocket->neighborPort = udpSocket->randomPort;

	mutex1.lock();
	myWants[udpSocket->originName] = 0;
	mutex1.unlock();
	
	timeoutTimer = new QTimer(this);
	timeoutTimer->start(1000);
	connect(timeoutTimer, SIGNAL(timeout()),
            this, SLOT(timeoutHandler())); 

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
	mutex1.lock();
	quint32 seqNo = myWants[origin].toInt();
	mutex1.unlock();

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
	inStream >> (qMap);
	if (qMap.contains("Want"))
	{
		QDataStream wantStream(&mapData, QIODevice::ReadOnly);
		wantStream >> statusMap;
		qDebug() << "Receive status msg: " << statusMap;
		mutex3.lock();
		pendingMsg.remove(statusMap["ACK"]["IS"].toString());
		mutex3.unlock();
		handleStatusMsg(statusMap["Want"], serverPort);
	}
	else if(qMap.contains("ChatText"))
	{
		qDebug() << "Receive rumor msg: " << qMap;
		handleRumorMsg(qMap, serverPort);
	}
	else
	{
		qDebug() << "Receive unrecognized msg" << qMap;
	}
	// mTimeoutTimer->stop();
    	if (udpSocket->hasPendingDatagrams()) {
		qDebug() << "!!!hasPendingDatagrams \n";
		goto RECV;
	}
}


// if timer fires, send out status message
void ChatDialog::antiEntropyHandler() {
	qDebug() << "AntiEntropyHandler called";
	writeStatusMessage(udpSocket->randomPort, "null", -1);
	antiEntropyTimer->start(10 * 1000);
	udpSocket->changeRandomPort();
}


void ChatDialog::timeoutHandler() {
	qDebug() << "TimeoutHandler called";
	mutex3.lock();
	// TODO: don't work on pendingMsg while traversing it.
	for (QVariantMap::const_iterator iter = pendingMsg.begin(); iter != pendingMsg.end(); ++iter) {
		if (iter.value().toInt() == (quint32) 0) {
			pendingMsg[iter.key()] = 1;
		} else if (iter.value().toInt() == (quint32) 1) {
			pendingMsg[iter.key()] = 2;
		} else if (iter.value().toInt() <= (quint32) 5) {
			int pos = 0;
			std::string port, origin, seqNo, delimiter = "$", s = iter.key().toStdString();

			pos = s.find(delimiter);
			port = s.substr(0, pos);
			s.erase(0, pos + delimiter.length());

			pos = s.find(delimiter);
			origin = s.substr(0, pos);
			s.erase(0, pos + delimiter.length());

			seqNo = s;

			QString Qorigin = QString::fromStdString(origin);
			quint16 Qport = QString::fromStdString(port).toInt();
			quint32 QseqNo = QString::fromStdString(seqNo).toInt();
			mutex2.lock();
			QString Qtext = allMessages[Qorigin][QseqNo];
			mutex2.unlock();

			writeRumorMessage(Qorigin, QseqNo, Qtext, Qport, false);
		} else {
			pendingMsg.remove[iter.key()];
		}
			
	}
	mutex3.unlock();
	timeoutTimer->start(1000);
}

void ChatDialog::handleRumorMsg(QVariantMap &rumorMap, quint16 port) {
	udpSocket->neighborPort = port;
	qDebug() << "handle rumor: " << rumorMap;
	QString text = rumorMap["ChatText"].toString();
	QString origin = rumorMap["Origin"].toString();
	quint32 seqNo = rumorMap["SeqNo"].toInt();

	// Process msg from other hosts
	if (origin != udpSocket->originName) {
		mutex1.lock();
		if (!myWants.contains(origin)) {
			// new host appear
		    	myWants[origin] = 0;
		}
		if (seqNo == (quint32) myWants[origin].toInt()) {
			writeRumorMessage(origin, seqNo, text, udpSocket->resendRumorPort(port), true);
		}
		mutex1.unlock();		
	}
	writeStatusMessage(port, origin, seqNo);
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
		port = udpSocket->neighborPort;
	}

	if (addToMsg) addToMessages(qMap);
	
	udpSocket->sendUdpDatagram(qMap, port);
	mutex1.lock();
	qDebug() << "!!!!!!!!!!db1\n";
	if ((quint32) myWants[origin].toInt() == seqNo) {
		myWants[origin] = myWants[origin].toInt() + 1;
	}
	mutex1.unlock();
	qDebug() << "!!!!!!!!!!db2\n";
	mutex3.lock();
	QString needAck = QString::number(port) + "$" +  origin + "$" + QString::number(seqNo);
	if (!pendingMsg.contains(needAck)) pendingMsg[needAck] = 0;
	mutex3.unlock();
}

void ChatDialog::handleStatusMsg(QVariantMap &gotWants, quint16 port) {
	udpSocket->neighborPort = port;
	qDebug() << "handle wants: " << gotWants << "\nfrom port : " << port;
	for (QVariantMap::const_iterator iter = gotWants.begin(); iter != gotWants.end(); ++iter) {
		qDebug() << iter.key() << iter.value();
		mutex1.lock();
		if (!myWants.contains(iter.key())) {
			myWants[iter.key()] = 0;
		}
		if (myWants[iter.key()].toInt() < iter.value().toInt()) {
			// Send Status back, need more msg
			writeStatusMessage(port, "null", -1);
			return;
		} else if (myWants[iter.key()].toInt() > iter.value().toInt()) {
			// Send rumor back
			QString origin = iter.key();
			mutex2.lock();
			QString message = allMessages[iter.key()][iter.value().toInt()];
			mutex2.unlock();
			quint32 seqNo = iter.value().toInt();
			writeRumorMessage(origin, seqNo, message, port, false);
			return;
		}
		mutex1.unlock();
	}
}

void ChatDialog::writeStatusMessage(int port, QString origin, quint32 seqNo)
{
	QMap<QString, QVariantMap> statusMap;
	mutex1.lock();
	statusMap["Want"] = myWants;
	QVariantMap tmpMap;
	tmpMap["IS"] = QString::number(udpSocket->myPort) + "$" +  origin + "$" + QString::number(seqNo);
	statusMap["ACK"] = tmpMap;
	mutex1.unlock();
	qDebug() << "Sending Status: " << statusMap;
	udpSocket->sendUdpDatagram(statusMap, port);
}


void ChatDialog::addToMessages(QVariantMap &qMap)
{
	qDebug() << "!!!!!!!!!!db5\n";
	QString message = qMap["ChatText"].toString();
	QString origin = qMap["Origin"].toString();
	quint32 seqNo = qMap["SeqNo"].toInt();
	
	if (message.isEmpty()) return;
	mutex2.lock();
	qDebug() << "!!!!!!!!!!db3\n";
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
	mutex2.unlock();
	qDebug() << "!!!!!!!!!!db4\n";
	this->textview->append(origin + ">: " + message);
	qDebug() << "!!!!!!!!!!db6\n";

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

void NetSocket::changeRandomPort(){
	if (randomPort < myPortMin || randomPort > myPortMax) {
		randomPort = myPort;
	}
	randomPort++;
	if (randomPort == myPort) {
		randomPort++;
	}
	if (randomPort > myPortMax) {
		randomPort = myPortMin;
	}
}

int NetSocket::resendRumorPort(int port){
	int newPort = port;
	newPort++;
	if (newPort == myPort) {
		newPort++;
	}
	if (newPort > myPortMax) {
		newPort = myPortMin;
	}
	return newPort;
}

/*
int NetSocket::getWritePort()
{
	// Determine which port to send to
	//sendPort = myPort == myPortMin ? myPort + 1 :myPort == myPortMax ? myPort - 1 :(genRandNum() % 2) == 0 ? myPort + 1: myPort - 1;
	
	for (int p = myPortMin; p <= myPortMax; p++) {
		// if is in use
		sendPort = p;
	} 
	sendPort = rand()%((myPortMax - myPortMin) + 1) + myPortMin;
    	qDebug() << "Send to Port: " << QString::number(sendPort);
   	return sendPort;
	
}
*/

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

