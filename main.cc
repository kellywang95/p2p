
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QDateTime>
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

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() <<"db1";
	QString origin = udpSocket->originName;
	qDebug() <<"db2"<< origin;
	QString message = textline->text();
	quint32 seqNo = myWants[origin];
	qDebug() <<"db3";
/*
	//if (myWants.contains(origin)) {
	//	seqNo = myWants[origin];
	//	myWants[origin]++;
	//} else {
	//	seqNo = 0;
	//	myWants.insert(origin, 1);
	} 
 */
	qDebug() <<"seqNo:" << seqNo; 
	myWants[origin]++;
	writeRumorMessage(origin, seqNo, message);
	qDebug() << "FIX: send message to other peers: " << message;
	
	textview->append(message);
	// Clear the textline to get ready for the next input message.
	textline->clear();
}


void ChatDialog::writeRumorMessage(QString &origin, quint32 seqNo, QString &text)
{
	QVariantMap qMap;
	qMap["ChatText"] = text;
	qMap["origin"] = origin;
	qMap["seqNo"] = seqNo;
	qDebug << "Sending message" << text;
	
	

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

NetSocket::~NetSocket()
{
}

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
    	qDebug() << "Receiver Port: " << QString::number(sendPort);
   	return sendPort;
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

