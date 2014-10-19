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
#define DEST_KEY "Dest"
#define HOP_KEY "HopLimit"
#define LASTIP_KEY "LastIP"
#define LASTPORT_KEY "LastPort"
#define BLOCK_REQUEST_KEY "BlockRequest"
#define BLOCK_REPLY_KEY "BlockReply"
#define DATA_KEY "Data"
#define SEARCH_KEY "Search"
#define BUDGET_KEY "Budget"
#define SEARCH_REPLY_KEY "SearchReply"
#define MATCH_NAME_KEY "MatchNames"
#define MATCH_ID_KEY "MatchIDs"
#define HOPLIMIT 10

quint32 generateRandom() {
	qsrand(qrand()); return qrand(); 
}

void CustomTextEdit::keyPressEvent(QKeyEvent* e) {
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
	hostline->setPlaceholderText("Enter new Host here");
	//A combobox to display all know origins for sending private chats
	privatebox = new QComboBox(this);
	privatebox->addItem("Select Destination for PM...");

	//Lay out the widgets to appear in the main window.
	//For Qt widget and layout1 concepts see:
	//http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layout1s.html
	sharefilebutton = new QPushButton("Share file...",this);
	downloadfilebutton = new QPushButton("Download file...",this);

	//Make buttons not clickable on return Press
	sharefilebutton->setAutoDefault(false);
	downloadfilebutton->setAutoDefault(false);
	layout1 = new QVBoxLayout();
	layout1->addWidget(privatebox);
	layout1->addWidget(textview);
	layout1->addWidget(textline);
	layout1->addWidget(hostline);
	layout1->addWidget(downloadfilebutton);
	layout1->addWidget(sharefilebutton);
	 // setLayout(layout1);
	listview = new QListWidget(this);
	searchline = new QLineEdit(this);
	searchline->setPlaceholderText("Search files by keywords...");
	layout2 = new QVBoxLayout();
	layout2->addWidget(listview);
	layout2->addWidget(searchline);
	gridlayout = new QGridLayout();
	gridlayout->addLayout(layout1,0,0);
	gridlayout->addLayout(layout2,0,1);
	setLayout(gridlayout);
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
	connect(sharefilebutton,SIGNAL(released()),this,SLOT(handleShareFileButton()));
	//Register callback on download file button to open file sharing dialog
	connect(downloadfilebutton,SIGNAL(released()),this,SLOT(handleDownloadButton()));

	connect(searchline, SIGNAL(returnPressed()),this, SLOT(handleSearchLine()));

	connect(listview,SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		this,SLOT(handleItemDoubleClicked(QListWidgetItem*)));
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

	 rebroadcasttimer = new QTimer(this);
	 connect(rebroadcasttimer,SIGNAL(timeout()),this,SLOT(handleRebroadcastTimer()));
	 rebroadcasttimer->start(1000);

}

ChatDialog::~ChatDialog() {
	delete textview; delete textline; delete hostline; delete peerlist;
	delete responsetimer; delete entropytimer; delete layout1;
}

void ChatDialog::checkReceipt() {
	if (success)
		return;
		success = true;
		//If there is a rumor, continue rumormongering
		if (!currentrumor.isEmpty())
		 sendRumorMessage(currentrumor);
}

void ChatDialog::addPeer() {
	QStringList splitlist; splitlist.insert(0,hostline->text());
	addPeers(splitlist);
	hostline->clear();
	hostline->setPlaceholderText("Sent.Enter new host here");
}

void ChatDialog::handleSearchLine() {
	QString querystring = searchline->text();
	searchline->clear();
	searchline->setPlaceholderText("search in progress...");
	rebroadcastSearchRequest(querystring);
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
			peerlist->append(Peer(QHostAddress(splitlist[0]), newPort));
	}
}

void ChatDialog::handleSearch(QString searchrequest,QString source){
	QStringList splitlist = searchrequest.split(" ");
	QStringList::iterator i;
	QVariantList matchNames;
	QByteArray result="";
	for (i = splitlist.begin(); i!=splitlist.end();++i)
		filemanager.keywordSearch((*i),matchNames,result);
	if (!result.isEmpty())
		sendSearchReplyMessage(source,searchrequest,matchNames,result);

}

void ChatDialog::handleRebroadcastTimer(){
	QMapIterator<QString,quint32> i (lastbudgetmap);
	if (!i.hasNext())
		searchline->setPlaceholderText("Search files by keywords...");
	while(i.hasNext()){
		i.next();
		rebroadcastSearchRequest(i.key(),i.value()*2);
	}
}
void ChatDialog::rebroadcastSearchRequest(QString searchrequest, quint32 budget) {
	if (budget > 100 || searchresultmap.size()>=10){
		lastbudgetmap.remove(searchrequest);
		return;
	}

	lastbudgetmap.insert(searchrequest,budget);
	QVariantMap searchmap;
	searchmap[ORIGIN_KEY] = QVariant(origin);	
	searchmap[SEARCH_KEY] = QVariant(searchrequest);
	searchmap[BUDGET_KEY] = QVariant(budget); 
	sendSearchRequestMessage(searchmap);
}
void ChatDialog::sendSearchRequestMessage(QVariantMap searchmap) {
 quint32 budget = searchmap.value(BUDGET_KEY).toUInt();
 quint32 minbudget = budget/(peerlist->size() - 1);
 quint32 bonusleft = budget%(peerlist->size() - 1);
 QList<Peer>::iterator i;
 for (i=peerlist->begin();i!=peerlist->end();++i) {
 	if (minbudget + bonusleft <= 0)
 		break;
 	if (i->sender ==QHostAddress::LocalHost && i->senderport==sock.myPort)
 		continue;
 	searchmap.insert(BUDGET_KEY,minbudget+bonusleft);
 	writeToSocket(searchmap,i->senderport,i->sender);
 	if (bonusleft > 0)
 		bonusleft--;
  }
}
//TODO: Extract to a separate file
PrivateMessageDialog::PrivateMessageDialog() {
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

DownloadFileDialog::DownloadFileDialog(QStringList origins) {
	//set the default destination as empty
	destinationorigin="";
	//Populate list of destinations
	origins.insert(0,"Select Origin to Request...");
	fileidtext = new CustomTextEdit(this);
	destinationbox=new QComboBox(this);
	destinationbox->addItems(origins);
	reqbutton = new QPushButton("Click to Request/Hit Return",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(fileidtext);
	layout->addWidget(destinationbox);
	layout->addWidget(reqbutton);
	fileidtext->setFocus(Qt::ActiveWindowFocusReason);
	connect(destinationbox,SIGNAL(activated(int)),this,SLOT(handleSelectionChanged(int)));
	connect(reqbutton,SIGNAL(released()),this, SLOT(handleRequestButton()));
	connect(fileidtext, SIGNAL(returnPressed()),this, SLOT(handleRequestButton()));
	setLayout(layout);
}

void DownloadFileDialog::handleRequestButton(){
	emit downloadfile(destinationorigin, fileidtext->toPlainText().toAscii());
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

void DownloadFileDialog::handleSelectionChanged(int index){
	if (index==0)
		destinationorigin = "";
	else
		destinationorigin = destinationbox->itemText(index);
}

void ChatDialog::handleItemDoubleClicked(QListWidgetItem* item){
	// qDebug()<<"DOWNLOAD:"<<item->text();
	QPair<QString,QByteArray> temp = searchresultmap.value(item->text());
	QString destination = temp.first;
	QByteArray fileid = temp.second;
	requestedmetafiles.insert(fileid,qMakePair(destination,item->text()));
	sendBlockRequestMessage(destination,fileid);
}

void ChatDialog::handleShareFileButton() {
	filedialog = new QFileDialog(this);
	filedialog->setFileMode(QFileDialog::ExistingFiles);
	QStringList filenames;
	if (filedialog->exec())
	  filenames = filedialog->selectedFiles();
	//Call FileShareManager instance to handle files
	filemanager.addFiles(filenames);
	delete filedialog;
	// qDebug()<<filenames;
}
void ChatDialog::handleDownloadButton() {
	//create list of all nodes known
	QStringList knownorigins;
	QMapIterator<QString, QVariant> i(statusmap);
 	while (i.hasNext()) {
 		i.next();
    if (i.key()!=origin)
    	knownorigins.append(i.key());
 }
	sharedialog = new DownloadFileDialog(knownorigins);
	connect(sharedialog,SIGNAL(downloadfile(QString,QByteArray)),this,
		SLOT(handleDownloadFile(QString,QByteArray)));
	sharedialog->show();
}

void ChatDialog::handleDownloadFile(QString destination, QByteArray fileid) {
	//Close the dialog and free it
	sharedialog->close();
	delete sharedialog;
	// qDebug()<<"THIS"<<fileid;
	if (destination.isEmpty()){
		return;
	}
	//update requested metafiles
	requestedmetafiles.insert(QByteArray::fromHex(fileid),qMakePair(destination,QString("")));
	sendBlockRequestMessage(destination,QByteArray::fromHex(fileid));
}

void ChatDialog::sendBlockRequestMessage(QString destination, QByteArray hashval){
	QVariantMap downloadrequestmap;
	downloadrequestmap[ORIGIN_KEY] = QVariant(origin);	
	downloadrequestmap[DEST_KEY] = QVariant(destination);
	downloadrequestmap[HOP_KEY] = QVariant(HOPLIMIT);
	downloadrequestmap[BLOCK_REQUEST_KEY] = QVariant(hashval);
	// qDebug()<<"SENDING A BLOCK REQUEST FOR"<<hashval.toHex()<<"TO "<<destination; 								
	QPair<QHostAddress,quint16> nexthop = hophash.value(destination);
	writeToSocket(downloadrequestmap,nexthop.second,nexthop.first);
}
void ChatDialog::handleSendPm(QString privatetext)
{
	privatedialog->close();
	delete privatedialog;
	QVariantMap message;																	
	message[CHAT_KEY] = QVariant(privatetext);	
	message[ORIGIN_KEY] = QVariant(origin);	
	message[DEST_KEY] = QVariant(destinationorigin);
	message[HOP_KEY] = QVariant(HOPLIMIT); 								
	QPair<QHostAddress,quint16> nexthop = hophash.value(destinationorigin);
	writeToSocket(message,nexthop.second,nexthop.first);
	//qDebug()<<"PM:"<<privatetext<<destinationorigin<<origin;
}
void ChatDialog::handleSearchReplyRumor(QVariantMap map) {
	if (!map.contains(MATCH_NAME_KEY) || !map.contains(MATCH_ID_KEY))
		return;
	QVariantList filenames = map.value(MATCH_NAME_KEY).toList();
	QByteArray fileids = map.value(MATCH_ID_KEY).toByteArray();
	QString source = map.value(ORIGIN_KEY).toString();
	QVariantList::iterator i;
	for(i=filenames.begin();i!=filenames.end();++i){
		if (!searchresultmap.contains((*i).toString())){
		QString filename =(*i).toString();
		QByteArray fileid = fileids.left(HASHSIZE);
		searchresultmap.insert(filename,qMakePair(source,fileid));
		listview->addItem(filename); 
		}
		fileids = fileids.mid(HASHSIZE);
	}

}
void ChatDialog::sendSearchReplyMessage(QString source, QString querystring,
 QVariantList matchNames, QByteArray result) {
	QVariantMap blockreplymap;
	blockreplymap[ORIGIN_KEY] = QVariant(origin);
	blockreplymap[DEST_KEY] = QVariant(source);
	blockreplymap[HOP_KEY] = QVariant(HOPLIMIT);
	blockreplymap[SEARCH_REPLY_KEY] = QVariant(querystring);
	blockreplymap[MATCH_NAME_KEY] = QVariant(matchNames);
	blockreplymap[MATCH_ID_KEY] = QVariant(result);
	QPair<QHostAddress,quint16> nexthop = hophash.value(source);
	writeToSocket(blockreplymap,nexthop.second,nexthop.first);
}


void ChatDialog::sendBlockReplyMessage(QByteArray block, QByteArray hashval, QString destination){
	QVariantMap blockreplymap;
	blockreplymap[ORIGIN_KEY] = QVariant(origin);
	blockreplymap[DEST_KEY] = QVariant(destination);
	blockreplymap[HOP_KEY] = QVariant(HOPLIMIT);
	blockreplymap[DATA_KEY] = QVariant(block);
	blockreplymap[BLOCK_REPLY_KEY] = QVariant(hashval);
	QPair<QHostAddress,quint16> nexthop = hophash.value(destination);
	writeToSocket(blockreplymap,nexthop.second,nexthop.first);
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
	sendRumorToAllPeers(message);									//Send rumor
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
void ChatDialog::sendRumorToAllPeers(QVariantMap map)
{
  	int peerlistsize = peerlist->size();
	for(int i=0; i<peerlistsize; ++i)
  	writeToSocket(map,peerlist->at(i).senderport,peerlist->at(i).sender);
}
QVariantMap ChatDialog::readDeserializeDatagram(QHostAddress* sender, quint16* senderport){
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
void ChatDialog::handleStatusRumor(QVariantMap map, QHostAddress sender, quint16 senderport){
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

void ChatDialog::handleChatRouteRumor(QVariantMap map, QHostAddress sender, quint16 senderport){
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
      	sendRumorToAllPeers(map);
      else
			 sendRumorMessage(map);				 //Rumor-monger	
		}
		/******************DIRECT ROUTE RUMOR*************************/
		else if(incomingseqno == temp -1 && isrumordirect){
      //While we currently have this rumor, we will update our
      //route map to reflect that this is a direct route
        hophash.insert(incomingorigin,qMakePair(sender,senderport));
        if (!map.contains(CHAT_KEY))
        	sendRumorToAllPeers(map);
		}
		/******************PAST OR FUTURE RUMOR**********************/
		else {
			//If we are lagging too far behind or the rumor is not fresh, we 
			//send our status message to the sender to update us or them respectively
	  	sendStatusMessage(senderport,sender);
		 }
}

void ChatDialog::handleSearchRequestRumor(QVariantMap map){
	if (map.value(ORIGIN_KEY) == origin)
		return;
	QString querystring = map.value(SEARCH_KEY).toString();
	QString source = map.value(ORIGIN_KEY).toString();
	handleSearch(querystring,source); 
	quint32 budget = map.value(BUDGET_KEY).toUInt() - 1;
	map.insert(BUDGET_KEY,QVariant(budget));
	sendSearchRequestMessage(map);

}

 void ChatDialog::handlePrivateBlockRumor(QVariantMap map){
 	/*******************INTENDED FOR THIS NODE *******************/
		if(map.value(DEST_KEY) == origin) {
			if (map.contains(CHAT_KEY)){ //Private Msg
				textview->append(map.value(ORIGIN_KEY).toString() +
					":Private Message : " +	map.value(CHAT_KEY).toString());
			}
			else if (map.contains(BLOCK_REQUEST_KEY)) { //Block request
				QByteArray hashval = map.value(BLOCK_REQUEST_KEY).toByteArray();
				QByteArray* result = filemanager.findBlockFromHash(hashval);
				if (result==NULL)
					qDebug()<<"Err: block not found";
				if(result!=NULL && filemanager.sanityCheck(hashval,(*result))){
					sendBlockReplyMessage(*result,hashval,map.value(ORIGIN_KEY).toString());
				}
			}
			else if (map.contains(BLOCK_REPLY_KEY) && map.contains(DATA_KEY)){ //Block reply
				QByteArray data = map.value(DATA_KEY).toByteArray();
				QByteArray hashval = map.value(BLOCK_REPLY_KEY).toByteArray();
				QString source = map.value(ORIGIN_KEY).toString();
				if (filemanager.sanityCheck(hashval,data)){
					QByteArray nexthash="";
					if (requestedmetafiles.contains(hashval)) {
						QString filename = requestedmetafiles.value(hashval).second;
						requestedmetafiles.remove(hashval);
						 nexthash = filemanager.addDownload(hashval,data,source,filename);
					}
					else{
						nexthash = filemanager.addBlock(hashval,data,source);
					}
					if (!nexthash.isEmpty()){
						sendBlockRequestMessage(source,nexthash);
					}
				}
			}
			else if (map.contains(SEARCH_KEY)){
				// qDebug()<<"handle search request for "<<map.value(SEARCH_KEY).toString();
				handleSearchRequestRumor(map);
			}
			else if(map.contains(SEARCH_REPLY_KEY)){
				handleSearchReplyRumor(map);
			}
		}
		/******************INTENDED FOR ANOTHER NODE**************/
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
	QVariantMap map = readDeserializeDatagram(&sender,&senderport);
	//Check if the immediate sender is a new source by cycling through
	//peerlist. If it is, add it as peer for future communications.
	addNewPeer(sender,senderport);
	if (map.contains(STATUS_KEY))																//Status Message
		handleStatusRumor(map,sender,senderport);
	else if (map.contains(ORIGIN_KEY) && map.contains(SEQ_KEY))	//Rumor message
		handleChatRouteRumor(map,sender,senderport);
	else if (map.contains(SEARCH_KEY) && map.contains(ORIGIN_KEY))
		handleSearchRequestRumor(map);
	else if (map.contains(ORIGIN_KEY) && map.contains(DEST_KEY))//Private/Block Message
		handlePrivateBlockRumor(map);
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
	QCA::Initializer qcainit;						//Initialize Crypto library
	QApplication app(argc,argv);				//Initialize Qt toolkit
	ChatDialog dialog;									//Create an initial chat dialog window
	dialog.show();

	QStringList argumentlist = QCoreApplication::arguments();
	argumentlist.removeAt(0);
		dialog.setnoforwarding(false);
	dialog.addPeers(argumentlist);			//Add Peers entered on command line
																			
																			//Enter the Qt main loop;	 
	return app.exec();									//everything else is event driven
}
