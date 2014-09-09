
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QDateTime>

#include "main.hh"
 

#define CHAT_KEY "ChatText"
#define ORIGIN_KEY "Origin"
#define SEQ_KEY	"SeqNo" 
#define STATUS_KEY "Want"


void CustomTextEdit::keyPressEvent(QKeyEvent* e)
{
	// Handle both Return and Enter Key
	if (e->key() == Qt::Key_Return || 
		e->key() == Qt::Key_Enter) {
		//Emit return pressed signal
		emit returnPressed();
		//e->accept();
	}
	else {
		QTextEdit::keyPressEvent(e);
	}
} 


ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//TODO: text-entry box can hold multiple (say 2 or 3) lines
	textline = new CustomTextEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);
	//TODO: append hostname to origins
	origin = QString::number(generateRandom());
	statusmap[origin] = 1;
	seqno = 1;
	// EXERCISE 1
	// TODO: Cleanup 
	// setFocus: Current reason may be arbitrary?
	textline->setFocus(Qt::ActiveWindowFocusReason);

	// Create a UDP network socket
	// NetSocket sock;
	if (!(sock.bind()))
		exit(1);
	// Register a callback on the CustomTextEdit's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
	// Register a callback on QudpSocket's readyRead signal to 
	// read pending datagrams.
	connect(&sock, SIGNAL(readyRead()),this, SLOT(gotNewMessage()));


}
quint32 ChatDialog::generateRandom()
{
		qsrand(QDateTime::currentDateTime().toTime_t());
		return qrand(); 
}
//TODO: Refactor
void ChatDialog::gotReturnPressed()
{
	QVariantMap message;
	message[CHAT_KEY] = QVariant(textline->toPlainText());
	message[ORIGIN_KEY] = QVariant(origin);
	message[SEQ_KEY] = seqno++;
	//update owns statusmap
	statusmap.insert(origin,seqno);
	//update recvdmessages
	recvdmessagemap.insertMulti(origin, message);
	//update GUI
	textview->append(textline->toPlainText());
	textline->clear();
	//update current rumor
	currentrumor = message;
	//Send rumor
	sendRumorMessage(message);
	// // qDebug() << "FIX: send message to other peers: " 
	// // << textline->toPlainText();
	// //TODO: remove? Or ensure no self message sending
	// textview->append(textline->toPlainText());
	// //send a rumor message
	// sendRumorMessage(QVariant(textline->toPlainText()));

	// // Clear the textline to get ready for the next input message.
	// textline->clear();

}
QByteArray ChatDialog::serializeToByteArray(QVariantMap message)
{
	QByteArray data;
	QDataStream* stream = new QDataStream(&data, QIODevice::WriteOnly);
	(*stream) << message;
	delete stream;
	return data; 
}

// QVariantMap ChatDialog::deserializeToQVariantMap(QByteArray )
// {

// }
void ChatDialog::sendRumorMessage(QVariantMap message){
	//choose a random neighbour; linear topology for now
	quint16 rumorport;
	if (sock.myPort == sock.myPortMin)
		rumorport=sock.myPortMin+1;
	else if (sock.myPort == sock.myPortMax)
		rumorport = sock.myPortMax-1;
	else
		rumorport = sock.myPort + (generateRandom()%2 ? 1: -1);
	//Write Datagram to net socket 
	//rumorport = 45517;
	qDebug()<<"SENDING RUMOR"<<message<<rumorport;
	writeToSocket(serializeToByteArray(message),rumorport);
}
//Write Datagram to net socket 
void ChatDialog::writeToSocket(QByteArray data, quint16 port,  
	QHostAddress host)
{
	// int pmin = sock.getmyPortMin(), pmax = sock.getmyPortMax();
	// for (int p = pmin; p <= pmax; p++) {
	// 	sock.writeDatagram(data,QHostAddress::LocalHost, p);
	// }
	sock.writeDatagram(data,host,port);	

}

//TODO: make message a pointer
void ChatDialog::gotNewMessage()
{
	QByteArray *serializedarr = new QByteArray();
	serializedarr->resize(sock.pendingDatagramSize());
	QHostAddress sender;
  quint16 senderPort;
	sock.readDatagram(serializedarr ->data(),serializedarr->size(),
	 &sender, &senderPort);
	QDataStream instream(serializedarr,QIODevice::ReadOnly);
	QVariantMap map;
	instream >> map;
	delete serializedarr;
	//qDebug() <<"SENDER:" << sender << senderPort;
	// Check if rumor or status
	if (map.contains(STATUS_KEY)) {	//Status
		QVariantMap statmap=map[STATUS_KEY].toMap();
		qDebug()<<"GOT STATUS"<<map<<senderPort;
		QMapIterator<QString,QVariant> i (statmap);
		while (i.hasNext()) {
			i.next();
			origindt incomingorigin = i.key();
			quint32 incomingseqno = i.value().toUInt();
			//add new origins
			if (!statusmap.contains(incomingorigin)) {
				statusmap.insert(incomingorigin,1);
			}
			seqnodt currentseqno = statusmap[incomingorigin].toUInt();
			//compare sequence numbers
			qDebug()<<"incomingorigincheck"<<incomingorigin<<incomingseqno;
			if (incomingseqno < currentseqno) {
				//Send one more message
								qDebug()<<senderPort <<"is lagging behind.\n";
								qDebug()<<"MESSAGE MAP IS\n###############\n"<<recvdmessagemap;
				QVariantMap::iterator i = 
				recvdmessagemap.find(incomingorigin);
				while (i!=map.end() && i.key()==incomingorigin) {
					QVariantMap message = i.value().toMap(); 
					qDebug()<<"loop";
					qDebug()<<"checking:"<<message<<senderPort;
					if (!message.contains(SEQ_KEY))
						qDebug()<<"SOURCE OF ERROR";
					if (message.value(SEQ_KEY).toUInt() == incomingseqno){
						qDebug()<<"sending:"<<message<<senderPort;
						writeToSocket(serializeToByteArray(message), 
						 senderPort);
						break;
					}
					++i;
				}
				if (i==map.end())
					qDebug()<<"DON'T HAVE!";

			}
			else if (incomingseqno > currentseqno) {
				//Send own status
				sendStatusMessage(senderPort,sender);
			}
			else {
				if (!currentrumor.isEmpty() && generateRandom()%2)
					 sendRumorMessage(currentrumor);
			}
		}
	}
	else {	//Rumor
		
		QString textline = map[CHAT_KEY].toString();
		origindt incomingorigin = map[ORIGIN_KEY].toString();
		seqnodt incomingseqno = map[SEQ_KEY].toUInt();
		
		if (!statusmap.contains(incomingorigin)) {
			statusmap.insert(incomingorigin,1);
		}
		seqnodt temp = statusmap[incomingorigin].toUInt();
		if (incomingseqno == temp) {//Only rumor monger if caught up?
			//Only one rumor behind
			//Display new message
			qDebug()<<"ADDING RUMOR" << textline << incomingorigin << incomingseqno;
			textview->append(textline);
			//update sequence number
			statusmap.insert(incomingorigin,++temp);
			//update recvdmessages
			recvdmessagemap.insertMulti(incomingorigin, map);
			sendStatusMessage(senderPort,sender);
			sendRumorMessage(map);
		}
		else {//Send only Status in reply
	  	sendStatusMessage(senderPort,sender);
		 }
		 // qDebug() << "FIX: Message received" << textline
		 // <<  map[ORIGIN_KEY].toUInt() << map[SEQ_KEY].toUInt();
	}



	//sock.readNewDatagram();
	//update statusmap
	//if statusmap.contains(ORIGIN_KEY)
}

void ChatDialog::sendStatusMessage(quint16 senderPort, QHostAddress sender){
	QVariantMap sendstatusmap;
	sendstatusmap[STATUS_KEY] = statusmap; 
	//Send it off
	writeToSocket(serializeToByteArray(sendstatusmap),senderPort,sender);
}
NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;

		  // connect(this, SIGNAL(readyRead()),
    //          this, SLOT(readNewDatagram()));
}
QString NetSocket::readNewDatagram()
{

	// QByteArray *serializedarr = new QByteArray();
	// serializedarr->resize(QUdpSocket::pendingDatagramSize());
	// QUdpSocket::readDatagram(serializedarr ->data(),serializedarr->size());
	// QDataStream instream(serializedarr,QIODevice::ReadOnly);
	// QVariantMap map;
	// instream >> map;

	// // Check if rumor or status
	// if (map.contains(STATUS_KEY)) {	//Status
	// 	//TODO: LOOP THROUGH ALL THE HOSTS
	// 	QVariantMap statmap = map[STATUS_KEY];
	// 	//compare sequence numbers
	// 	//TODO: CHANGE ORIGIN TO ACTUAL HOST(S)
	// 	if (statmap[ORIGIN_KEY].toUInt() < SeqNo) {
	// 		//Send messages
	// 	}
	// 	else if (statmap[ORIGIN_KEY].toUInt() > SeqNo {
	// 		//Send own status (or send all of them together?)
	// 	}
	// 	else {
	// 		//Flip a coin to continue rumor
	// 	}
	// }
	// else {	//Rumor
	// 	QVariant message = map[CHAT_KEY];
	// 	QString textline = message.toString();
	// 	//update statusmap


	// 	 qDebug() << "FIX: Message received" << message 
	// 	 	<<  map[ORIGIN_KEY].toUInt() << map[SEQ_KEY].toUInt();
	// 	return textline;
	// }
	// delete serializedarr;
	return 0; //TODO: CHANGE RETURN TYPE
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

