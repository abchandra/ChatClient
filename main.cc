/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/
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

quint32 generateRandom()
{
	qsrand(qrand()); return qrand(); 
}

void CustomTextEdit::keyPressEvent(QKeyEvent* e)
{
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
		emit returnPressed();
	else 
		QTextEdit::keyPressEvent(e);
} 

ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");
	//Read-only text box where we display messages from everyone.
	//This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);
	//Small text-entry box the user can enter messages.
	//This widget normally expands only horizontally,
	//leaving extra vertical space for the textview widget.
	textline = new CustomTextEdit(this);
	//A small text-entry line for the user to add new peers
	hostline = new QLineEdit(this);
	hostline->insert("Enter new host here");
	//Lay out the widgets to appear in the main window.
	//For Qt widget and layout concepts see:
	//http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	layout->addWidget(hostline);
	setLayout(layout);

	//Generate Origin Key : Append a random number to hostname
	//Insert Origin & seqno to status map 
	qsrand(QDateTime::currentDateTime().toTime_t());
	origin = QHostInfo::localHostName() + QString::number(qrand());
	seqno = 1;
	statusmap.insert(origin, seqno);
	
	//Lab1 EXERCISE 1 
	//setFocus on the textline to for better UX
	textline->setFocus(Qt::ActiveWindowFocusReason);

	//find a port to bind our Netsocket element
	if (!(sock.bind()))
		exit(1);

	//Populate list of peers with all ports from myPortMin to myPortMax
	 peerlist = new QList<Peer>();
	for (int p = sock.myPortMin; p <= sock.myPortMax; p++) {
		if (p==sock.myPort)
			continue;
		peerlist->append(Peer(
			QHostAddress::LocalHost,p,QHostInfo::localHostName()));
	}
	
	//Register a callback on the CustomTextEdit's returnPressed signal
	//so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),this, SLOT(gotReturnPressed()));
	//Register a callback on NetSocket's readyRead signal to
	//read pending datagrams.
	connect(&sock, SIGNAL(readyRead()),this, SLOT(gotNewMessage()));
	//Register a callback on QLineEdit's returnPressed signal so that
	//we can parse and add the peer added by the user.
	connect(hostline, SIGNAL(returnPressed()),SLOT(addPeer()));

	//Register a callback on responsetimer's timeout() signal to take necessary
	//steps in case our rumor sending does not result in a status returned
	responsetimer =new QTimer(this);
	connect(responsetimer,SIGNAL(timeout()),this,SLOT(checkReceipt()));
	//Register a callback on entropytimer's timeout() signal to send a status
	//message to a random peer as part of anti-entropy implementation
	QTimer *entropytimer = new QTimer(this);
	connect(entropytimer,SIGNAL(timeout()),this,SLOT(sendAntiEntropyStatus()));
	entropytimer->start(10000);

}

void ChatDialog::checkReceipt(){
	if (!success)
		success=true;
		//If there is a rumor, randomly decide whether to continue rumormongering
		if (!currentrumor.isEmpty() && generateRandom()%2==0)
		 sendRumorMessage(currentrumor);
}

void ChatDialog::addPeer(){
	
	QStringList splitlist; splitlist.insert(0,hostline->text());
	addPeers(splitlist);
	hostline->clear();
	hostline->insert("Sent.Enter new host here");
}
void ChatDialog::addPeers(QStringList argvlist){
	int argc = argvlist.size();
	for (int i=0; i<argc; i++) {
		QStringList splitlist = argvlist[i].split(":");	//"host:port"|"ipadd:port"
		if (splitlist.size()!=2){												//Check Format
			qDebug()<<"Invalid format. Use host:port or ip:port";
			return;
		}
		bool isokay;
		quint16 newPort = splitlist[1].toUInt(&isokay);				
		if (!isokay){																	//Check for conversion errors
			qDebug()<<"Invalid format. Port is not numeric";
			return;
		}
		if(QHostAddress(splitlist[0]).isNull()){	//Check if convertable to ip
			unresolvedhostmap.insertMulti(splitlist[0],newPort);
			QHostInfo::lookupHost(splitlist[0],this,SLOT(lookedUp(QHostInfo)));
		}
		else																		 //ip usable. Insert as is.
			peerlist->append(Peer(QHostAddress(splitlist[0]),
				newPort));
	}
}
void ChatDialog::lookedUp(QHostInfo host)
{
	if (host.error() != QHostInfo::NoError) {
         qDebug() << "Lookup failed:" << host.errorString();
         return;
     }
     //If lookup succeeded, add the peers with matching hostnames to our 
     //peerlist; Also remove such peers from the unresolvedhostmap
     QString hostname = host.hostName();
     QHostAddress address = host.addresses()[0];
     QMap<QString,quint16>::iterator i = unresolvedhostmap.find(hostname);
     while (i!=unresolvedhostmap.end() && i.key()==hostname) {
     	 peerlist->append(Peer(address,i.value(),hostname));  	
     	 
   	 qDebug()<<"Peer added on lookup"<<hostname<<i.value()<<address;
     	 ++i; 
   	 }
   	 unresolvedhostmap.remove(hostname);
}

void ChatDialog::sendAntiEntropyStatus()
{
	if (!peerlist->size())
		return;
	Peer randompeer = chooseRandomPeer();
	qDebug()<<"Anti-entropy:"<<randompeer.host<<randompeer.senderPort;
	sendStatusMessage(randompeer.senderPort,randompeer.sender);
}

void ChatDialog::gotReturnPressed()
{
	QVariantMap message;																		//Prepare user input
	message[CHAT_KEY] = QVariant(textline->toPlainText());	//to a QVMap suitable
	message[ORIGIN_KEY] = QVariant(origin);									//for sending
	message[SEQ_KEY] = seqno++;
	statusmap.insert(origin,seqno);								//update own statusmap
	recvdmessagemap.insertMulti(origin, message);	//update recvdmessages
	textview->append(textline->toPlainText());		//update GUI
	textline->clear();
	currentrumor = message;												//update current rumor
	sendRumorMessage(message);										//Send rumor

}
QByteArray ChatDialog::serializeToByteArray(QVariantMap message)
{
	QByteArray data;
	QDataStream* stream = new QDataStream(&data, QIODevice::WriteOnly);
	(*stream) << message;
	delete stream;
	return data; 
}

Peer ChatDialog::chooseRandomPeer()
{
	return peerlist->at(generateRandom()%peerlist->size());
}


void ChatDialog::sendRumorMessage(QVariantMap message){
	//Choose a random peer to rumor monger to from known peers
	Peer randompeer = chooseRandomPeer();
	// qDebug()<<"Sending rumor to"<<randompeer.host<<randompeer.senderPort;
	//Re-initialize timer and success flag to timeout on failure
	responsetimer->start(2000); success = false;
	//Serialize the QVMap and send the datagram to randompeer
	writeToSocket(serializeToByteArray(message),
		randompeer.senderPort,randompeer.sender);
}

void ChatDialog::writeToSocket(QByteArray data, quint16 port,QHostAddress host)
{
	sock.writeDatagram(data,host,port);	
}

void ChatDialog::gotNewMessage()
{
	//Read Datagram(as a serialized QByteArray) and sender details 
	QByteArray *serializedarr = new QByteArray();
	serializedarr->resize(sock.pendingDatagramSize());
	QHostAddress sender;
  quint16 senderPort;
	sock.readDatagram(serializedarr ->data(),serializedarr->size(),
										&sender, &senderPort);

	//Deserialize the Datagram to a QVMap
	QDataStream instream(serializedarr,QIODevice::ReadOnly);
	QVariantMap map;
	instream >> map;
	delete serializedarr;

	//Check if the immediate sender is a new source by cycling through
	//peerlist. If it is, add it as peer for future communications.
	int peerlistsize = peerlist->size();
 	int i;
  for(i=0; i<peerlistsize; ++i) {
  	if ((peerlist->at(i).sender == sender) && 
  		(peerlist->at(i).senderPort == senderPort))
  		break;
  }
  if (i==peerlistsize){
  	// qDebug()<<"New Peer added" <<sender<<senderPort;
  	peerlist->append(Peer(sender,senderPort));
  }

	//Check what kind of datagram this is, and act accordingly
	if (map.contains(STATUS_KEY)) {									//Status Message
		success = true;																//Reset success for timeout	
		QVariantMap statmap=map[STATUS_KEY].toMap(); 	//Extract status

		//There may be peers who do not know which origins they want messages from.
		//As a result, we must add such origins to the statmap sent by them so
		//that we can effectively forward messages from those origins to them.
		//Note that this will lead to a infinte loop of status sending to a peer
		//who does not update their status map, but Ennan says that is fine.
		QMapIterator<QString,QVariant> j (statusmap);
		while (j.hasNext()) {
			j.next();
			if (!statmap.contains(j.key())) {	//Sender does not know of this node
				statmap.insert(j.key(),1);		//Add this node to their want statmap
			}
		}
		QMapIterator<QString,QVariant> i (statmap);		//iterate over all statuses
		while (i.hasNext()) {
			i.next();
			origindt incomingorigin = i.key();
			quint32 incomingseqno = i.value().toUInt();
		
			if (!statusmap.contains(incomingorigin)) {	//Add any unseen origins
				statusmap.insert(incomingorigin,1);
			}
			seqnodt currentseqno = statusmap.value(incomingorigin).toUInt();
			//compare sequence numbers to decide what action to take
			if (incomingseqno < 1)
				continue;
			if (incomingseqno < currentseqno) {				//Sender is missing messages
				//Send the next message in sequence to sender
				QVariantMap::iterator i = 
				recvdmessagemap.find(incomingorigin);
				while (i!=map.end() && i.key()==incomingorigin) {
					QVariantMap message = i.value().toMap(); 
					if (!message.contains(SEQ_KEY)){
						qDebug()<<"Err: Our Message map is corrupted";
						continue;
					}
					if (message.value(SEQ_KEY).toUInt() == incomingseqno) {
						writeToSocket(serializeToByteArray(message), senderPort, sender);
						break;
					}
					++i;
				}
				if (i==map.end())
					qDebug()<<"Err: Our message seems to be expired";	
			}
			else if (incomingseqno > currentseqno) 				//We are lagging behind
				sendStatusMessage(senderPort,sender);				//Send own status
			else if (!currentrumor.isEmpty() && generateRandom()%2==0) //Coin flip
					 sendRumorMessage(currentrumor);											 //Rumor monger
			else
				currentrumor.clear(); 																	 //Cease monger
		}
	}
	else {																						//Rumor Message
		
		QString textline = map[CHAT_KEY].toString();			
		origindt incomingorigin = map[ORIGIN_KEY].toString();
		seqnodt incomingseqno = map[SEQ_KEY].toUInt();
		
		if (!statusmap.contains(incomingorigin)) {			//Add any unseen origins
			statusmap.insert(incomingorigin,1);
		}
		seqnodt temp = statusmap[incomingorigin].toUInt();
		if (incomingseqno == temp) {	
			textview->append(incomingorigin + ":"+textline);	//Display the new rumor
			statusmap.insert(incomingorigin,++temp);					//update sequence number
			recvdmessagemap.insertMulti(incomingorigin, map);	//update recvdmessages
			sendStatusMessage(senderPort,sender);							//Reply with our status
			sendRumorMessage(map);														//Rumor-monger	
		}
		else {
			//If we are lagging too far behind, we just ignore the message
			//we have just received and send our own status message to update
	  	sendStatusMessage(senderPort,sender);
		 }
	}
}

void ChatDialog::sendStatusMessage(quint16 senderPort, QHostAddress sender){
	QVariantMap sendstatusmap;
	sendstatusmap[STATUS_KEY] = statusmap; 
	//Send it off
	writeToSocket(serializeToByteArray(sendstatusmap),senderPort,sender);
}
NetSocket::NetSocket()
{
	//Pick a range of four UDP ports to try to allocate by default,
	//computed based on my Unix user ID.This makes it trivial for up 
	//to four Peerster instances per user to find each other on the 
	//same host, barring UDP port conflicts with other applications 
	//(which are quite possible).We use the range from 32768 to 49151.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

bool NetSocket::bind()
{
	//Try to bind to each of the range myPortMin to myPortMax in turn.
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
	QApplication app(argc,argv);				//Initialize Qt toolkit

	ChatDialog dialog;									//Create an initial chat dialog window
	dialog.show();

	QStringList argumentlist = QCoreApplication::arguments();
	argumentlist.removeAt(0);
	dialog.addPeers(argumentlist);			//Add Peers entered on command line
																			
																			//Enter the Qt main loop;	 
	return app.exec();									//everything else is event driven
}

