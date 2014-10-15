/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/
#include <unistd.h>

#include <QApplication>
#include <QDebug>
#include <QDateTime>

#include "main.hh"

#define CHAT_KEY "ChatText"
#define ORIGIN_KEY "Origin"
#define SEQ_KEY	"SeqNo" 
#define STATUS_KEY "Want"
#define SOURCE_KEY "Origin"
#define DEST_KEY "Dest"
#define HOP_KEY "HopLimit"
#define LASTIP_KEY "LastIP"
#define LASTPORT_KEY "LastPort"

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
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::AnyFile);
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
	//A combobox to display all know origins for sending private chats
	privatebox = new QComboBox(this);
	privatebox->addItem("Select Destination for PM...");
	//Lay out the widgets to appear in the main window.
	//For Qt widget and layout concepts see:
	//http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	filebutton = new QPushButton("Share file...",this);

	layout = new QVBoxLayout();
	layout->addWidget(privatebox);
	layout->addWidget(textview);
	layout->addWidget(textline);
	layout->addWidget(hostline);
	layout->addWidget(filebutton);
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
	//Register callback on selection of an origin on the combobox
	connect(privatebox,SIGNAL(activated(int)),this,SLOT(handleSelectionChanged(int)));
	//Register callback on file share button to open file sharing dialog
	connect(filebutton,SIGNAL(released()),this,SLOT(handleFileButton()));
	//Register a callback on responsetimer's timeout() signal to take necessary
	//steps in case our rumor sending does not result in a status returned
	responsetimer =new QTimer(this);
	connect(responsetimer,SIGNAL(timeout()),this,SLOT(checkReceipt()));
	responsetimer->setSingleShot(true);
	//Register a callback on entropytimer's timeout() signal to send a status
	//message to a random peer as part of anti-entropy implementation
	entropytimer = new QTimer(this);
	connect(entropytimer,SIGNAL(timeout()),this,SLOT(sendAntiEntropyStatus()));
	entropytimer->start(10000);

	 routetimer = new QTimer(this);
	 connect(routetimer,SIGNAL(timeout()),this,SLOT(sendRouteRumor()));
	 routetimer->start(60000);
	 sendRouteRumor();
}

ChatDialog::~ChatDialog()
{
	delete textview; delete textline; delete hostline; delete peerlist;
	delete responsetimer; delete entropytimer; delete layout;
}

void ChatDialog::checkReceipt()
{
	if (success)
		return;
		success = true;
		//If there is a rumor, continue rumormongering
		if (!currentrumor.isEmpty())
		 sendRumorMessage(currentrumor);
}

void ChatDialog::addPeer()
{
	QStringList splitlist; splitlist.insert(0,hostline->text());
	addPeers(splitlist);
	hostline->clear();
	hostline->insert("Sent.Enter new host here");
}
void ChatDialog::addPeers(QStringList argvlist){
	int argc = argvlist.size();
	for (int i=0; i<argc; i++) {
		if (argvlist[i]=="-noforward") {
			setnoforwarding(true);
			continue;
		}
		QStringList splitlist = argvlist[i].split(":");	//"host:port"|"ipadd:port"
		if (splitlist.size()!=2){												//Check Format
			qDebug()<<"Invalid format. Use host:port or ip:port";
			continue;
		}
		bool isokay;
		quint16 newPort = splitlist[1].toUInt(&isokay);				
		if (!isokay){																	//Check for conversion errors
			qDebug()<<"Invalid format. Port is not numeric";
			continue;
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
//TODO: Extract to a separate file
PrivateMessageDialog::PrivateMessageDialog()
{
	setWindowTitle("New Private Message");
	privatetext = new CustomTextEdit(this);
	sendbutton = new QPushButton("Click to Send/Hit Return",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(privatetext);
	layout->addWidget(sendbutton);
	privatetext->setFocus(Qt::ActiveWindowFocusReason);
	connect(sendbutton,SIGNAL(released()),this,SLOT(handleSendButton()));
	connect(privatetext, SIGNAL(returnPressed()),this, SLOT(handleSendButton()));
	setLayout(layout);
}

void ChatDialog::handleSelectionChanged(int index)
{
	if (index == 0)
		return;
	destinationorigin = privatebox->itemText(index);
	privatedialog = new PrivateMessageDialog();
	connect(privatedialog,SIGNAL(sendpm(QString)),this,
			SLOT(handleSendPm(QString)));
	privatebox->setCurrentIndex(0);
	privatedialog->show();
}
void ChatDialog::handleFileButton(){
	filedialog = new QFileDialog(this);
	filedialog->setFileMode(QFileDialog::ExistingFiles);
	QStringList fileNames;
	if (filedialog->exec())
	  fileNames = filedialog->selectedFiles();
	qDebug()<<fileNames;
}
void ChatDialog::handleSendPm(QString privatetext)
{
	privatedialog->close();
	delete privatedialog;
	QVariantMap message;																	
	message[CHAT_KEY] = QVariant(privatetext);	
	message[SOURCE_KEY] = QVariant(origin);	
	message[DEST_KEY] = QVariant(destinationorigin);
	message[HOP_KEY] = QVariant(10); 								
	QPair<QHostAddress,quint16> nexthop = hophash.value(destinationorigin);
	writeToSocket(message,nexthop.second,nexthop.first);
	//qDebug()<<"PM:"<<privatetext<<destinationorigin<<origin;
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
	//qDebug()<<"Anti-entropy:"<<randompeer.host<<randompeer.senderport;
	sendStatusMessage(randompeer.senderport,randompeer.sender);
}

void ChatDialog::sendRouteRumor()
{
	QVariantMap message;
	message[ORIGIN_KEY] = QVariant(origin);				//Ready route rumor
	message[SEQ_KEY] = seqno++;
	statusmap.insert(origin,seqno);								//Update own statusmap
	recvdmessagemap.insertMulti(origin, message);	//Update recvdmessages	
	SendRumorToAllPeers(message);									//Send rumor
}
void ChatDialog::gotReturnPressed()
{
	QVariantMap message;																		//Prepare user input
	message[CHAT_KEY] = QVariant(textline->toPlainText());	//to a QVMap suitable
	message[ORIGIN_KEY] = QVariant(origin);									//for sending
	message[SEQ_KEY] = seqno++;
	statusmap.insert(origin,seqno);								//Update own statusmap
	recvdmessagemap.insertMulti(origin, message);	//Update recvdmessages
	textview->append(textline->toPlainText());		//Update GUI
	textline->clear();
	currentrumor = message;												//Update current rumor
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
	// qDebug()<<"Sending rumor to"<<randompeer.host<<randompeer.senderport;
	//Re-initialize timer and success flag to timeout on failure

	responsetimer->start(2000); success = false;
	//Serialize the QVMap and send the datagram to randompeer
	writeToSocket(message,
		randompeer.senderport,randompeer.sender);
}

void ChatDialog::writeToSocket(QVariantMap message, quint16 port,QHostAddress host)
{
	if (message.contains(CHAT_KEY) && noforwarding)
		return;
	QByteArray data = serializeToByteArray(message);
	sock.writeDatagram(data,host,port);	
    // qDebug()<<"RUMOR:"<<message<<"\n"<<"DESTINATION:"<<host<<port;
}
void ChatDialog::addNewPeer(QHostAddress sender, quint16 senderport){
  int peerlistsize = peerlist->size();
 	int i;
  for(i=0; i<peerlistsize; ++i) {
  	if ((peerlist->at(i).sender == sender) && 
  		(peerlist->at(i).senderport == senderport))
  		break;
  }
  if (i==peerlistsize){
    qDebug()<<"New Peer added" <<sender<<senderport;
  	peerlist->append(Peer(sender,senderport));
  }
}
void ChatDialog::SendRumorToAllPeers(QVariantMap map)
{
  	int peerlistsize = peerlist->size();
	for(int i=0; i<peerlistsize; ++i)
  	writeToSocket(map,peerlist->at(i).senderport,peerlist->at(i).sender);
}
QVariantMap ChatDialog::ReadDeserializeDatagram(QHostAddress* sender, quint16* senderport){
	//Read Datagram(as a serialized QByteArray) and sender details 
	QByteArray *serializedarr = new QByteArray();
	serializedarr->resize(sock.pendingDatagramSize());
	sock.readDatagram(serializedarr ->data(),serializedarr->size(),
										sender, senderport);

	//Deserialize the Datagram to a QVMap
	QDataStream instream(serializedarr,QIODevice::ReadOnly);
	QVariantMap map;
	instream >> map;
	delete serializedarr;
	return map;	
}
void ChatDialog::HandleStatusRumor(QVariantMap map, QHostAddress sender, quint16 senderport){
			success = true;																//Reset success for timeout	
		QVariantMap statmap=map[STATUS_KEY].toMap(); 	//Extract status

		//There may be peers who do not know which origins they want messages from.
		//As a result, we must add such origins to the statmap sent by them so
		//that we can effectively forward messages from those origins to them.
		QMapIterator<QString,QVariant> j (statusmap);
		while (j.hasNext()) {
			j.next();
			if (!statmap.contains(j.key())) {	//Sender does not know of this node
				statmap.insert(j.key(),1);			//Add this node to their want statmap
			}
		}
		QMapIterator<QString,QVariant> i (statmap);			//iterate over all statuses
		while (i.hasNext()) {
			i.next();
			QString incomingorigin = i.key();
			quint32 incomingseqno = i.value().toUInt();
			/*********************ADD UNSEEN ORIGINS***************/
			if (!statusmap.contains(incomingorigin)) {
				statusmap.insert(incomingorigin,1);
				privatebox->addItem(incomingorigin);
			}
			/************COMPARE SEQUENCE NUMBERS*****************/
			quint32 currentseqno = statusmap.value(incomingorigin).toUInt();
			if (incomingseqno < 1)
				continue;
			if (incomingseqno < currentseqno) {					//Sender is missing messages
																									//Send the next message
				QVariantMap::iterator i = recvdmessagemap.find(incomingorigin);
				while (i!=map.end() && i.key()==incomingorigin) {
					QVariantMap message = i.value().toMap(); 
					if (!message.contains(SEQ_KEY)){
						qDebug()<<"Err: Our Message map is corrupted";
						continue;
					}
					if (message.value(SEQ_KEY).toUInt() == incomingseqno) {
						writeToSocket(message, senderport, sender);
						break;
					}
					++i;
				}
				if (i==map.end())
					qDebug()<<"Err: The message seems to have expired";	
			}
			else if (incomingseqno > currentseqno) 				//We are lagging behind
				sendStatusMessage(senderport,sender);				//Send own status
			else if (!currentrumor.isEmpty() && generateRandom()%2==1) //Coin flip
					 sendRumorMessage(currentrumor);											 //Rumor monger
			else
				currentrumor.clear(); 																	 //Cease monger
		}
}

void ChatDialog::HandleChatRouteRumor(QVariantMap map, QHostAddress sender, quint16 senderport){
	QString incomingorigin = map.value(ORIGIN_KEY).toString();
		quint32 incomingseqno = map.value(SEQ_KEY).toUInt();
		/******************NAT TRAVERSAL KEYS LOOKED UP *************/
		//Check for LASTIP_KEY and LASTPORT_KEY for NAT travesal
    bool isrumordirect = false;
		if (map.contains(LASTIP_KEY) && map.contains(LASTPORT_KEY)){		
			quint16 lastport = map.value(LASTPORT_KEY).toUInt();			
			QHostAddress lastip = QHostAddress(map.value(LASTIP_KEY).toUInt());
			addNewPeer(lastip,lastport);
		}
    else 
      isrumordirect = true;  
    //Overwrite lastIP and lastPort for NAT Traversal
    map.insert(LASTIP_KEY,sender.toIPv4Address());
    map.insert(LASTPORT_KEY,senderport);
   /************************************************************/

    /*********************ADD UNSEEN ORIGINS*******************/
		if (!statusmap.contains(incomingorigin)) {
			statusmap.insert(incomingorigin,1);
			privatebox->addItem(incomingorigin);
		}

		quint32 temp = statusmap.value(incomingorigin).toUInt();
		/*******************FRESH RUMOR*********************************/
		if (incomingseqno == temp) {

				/********CHAT MESSAGE:UPDATE GUI*******************/
				if (map.contains(CHAT_KEY)) {											//Chat rumor				
				QString textline = map.value(CHAT_KEY).toString();		
				textview->append(incomingorigin + ":"+textline);	//Display new rumor
			}
			/****************UPDATE VARIOUS MAPS**************/
			recvdmessagemap.insertMulti(incomingorigin, map);	//update recvdmessages
			temp++;																						//update sequence number
			statusmap.insert(incomingorigin,temp);						//Insert in Status map
			hophash.insert(incomingorigin,qMakePair(sender,senderport));//Update Hop
			sendStatusMessage(senderport,sender);							//Reply with our status
			/**********ROUTEMONGER OR RUMORMONGER**************/
      if (!map.contains(CHAT_KEY)) 		//Route messages must be sent to all peers
      	SendRumorToAllPeers(map);
      else
			 sendRumorMessage(map);				 //Rumor-monger	
		}
		/******************DIRECT ROUTE RUMOR*************************/
		else if(incomingseqno == temp -1 && isrumordirect){
      //While we currently have this rumor, we will update our
      //route map to reflect that this is a direct route
        hophash.insert(incomingorigin,qMakePair(sender,senderport));
        if (!map.contains(CHAT_KEY))
        	SendRumorToAllPeers(map);
		}
		/******************PAST OR FUTURE RUMOR**********************/
		else {
			//If we are lagging too far behind or the rumor is not fresh, we 
			//send our status message to the sender to update us or them respectively
	  	sendStatusMessage(senderport,sender);
		 }
}
 void ChatDialog::HandlePrivateMessageRumor(QVariantMap map){
 	/*******************PM INTENDED FOR THIS NODE ****************/
		if (map.value(DEST_KEY) == origin && map.contains(CHAT_KEY)){
			textview->append(map.value(SOURCE_KEY).toString() +
				":Private Message : " +	map.value(CHAT_KEY).toString());
		}
		/******************PM INTENDED FOR ANOTHER NODE**************/
		else if (map.contains(HOP_KEY)){
			quint32 hopsremaining = map.value(HOP_KEY).toUInt() - 1;
			if (hopsremaining > 0) {
				map.insert(HOP_KEY,hopsremaining);
				QPair<QHostAddress,quint16> nexthop =
					hophash.value(map.value(DEST_KEY).toString());
				writeToSocket(map,nexthop.second,nexthop.first);
			}
		}
 }

void ChatDialog::gotNewMessage()
{
	//Read Datagram(as a serialized QByteArray) and retrieve sender details
	QHostAddress sender; quint16 senderport; 
	QVariantMap map = ReadDeserializeDatagram(&sender,&senderport);
	//Check if the immediate sender is a new source by cycling through
	//peerlist. If it is, add it as peer for future communications.
	addNewPeer(sender,senderport);
	if (map.contains(STATUS_KEY))																//Status Message
		HandleStatusRumor(map,sender,senderport);
	else if (map.contains(ORIGIN_KEY) && map.contains(SEQ_KEY))	//Rumor message
		HandleChatRouteRumor(map,sender,senderport);
	else if (map.contains(SOURCE_KEY) && map.contains(DEST_KEY))//Private Message
		HandlePrivateMessageRumor(map);
	else																											 //Invalid Message
		qDebug()<<"Invalid message:"<<map;	
}

void ChatDialog::sendStatusMessage(quint16 senderport, QHostAddress sender){
	QVariantMap sendstatusmap;
	sendstatusmap[STATUS_KEY] = statusmap; 
	writeToSocket(sendstatusmap,senderport,sender);
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
	// QCA::Initializer qcainit;						//Initialize Crypto library
	ChatDialog dialog;									//Create an initial chat dialog window
	dialog.show();

	QStringList argumentlist = QCoreApplication::arguments();
	argumentlist.removeAt(0);
		dialog.setnoforwarding(false);
	dialog.addPeers(argumentlist);			//Add Peers entered on command line
																			
																			//Enter the Qt main loop;	 
	return app.exec();									//everything else is event driven
}
