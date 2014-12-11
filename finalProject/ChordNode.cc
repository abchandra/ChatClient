#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>

#define JOIN_KEY "Join"
#define isIn(a,b,c) (a>=b && a<c)
ChordNode::ChordNode(){
	setWindowTitle("Chordster");

	sharefilebutton = new QPushButton("Share file...",this);
	downloadfilebutton = new QPushButton("Download file...",this);

	//Make buttons not clickable on return Press
	sharefilebutton->setAutoDefault(false);
	downloadfilebutton->setAutoDefault(false);

	layout1 = new QVBoxLayout();
	layout1->addWidget(downloadfilebutton);
	layout1->addWidget(sharefilebutton);
	setLayout(layout1);
	//Bind to a port
	if (!(sock.bind()))
		exit(1);
	//Lookup node IP address
	if (!(sock.LookupOwnIP()))
		exit(1);
	//Create identifier for node
	setCreateChordKey();
	finger = new fingerTableEntry[idlength];
	//Register a callback on NetSocket's readyRead signal to
	//read pending datagrams.
	// connect(&sock, SIGNAL(readyRead()),this, SLOT(gotNewMessage()));
	
	//Dialog to connect to a chord node

	//Here we need preserve join conditions from 4.4
	//For now:
	//	locate succ and pred of succ
	//	add self to between
	//	switch keys around
	//	Will need a stabilization state
	//	
}


void ChordNode::setCreateChordKey(){
 	if(!sock.hostInfo)
 		return;
	QHostAddress address = sock.hostInfo->addresses()[0];
	quint16 port = sock.myPort;
	QString nodeName = address.toString() + ":" + QString::number(port);
  quint32 nodeKey = getKeyFromName(nodeName);
  selfNode = new Node(address,port,nodeKey);
  // nodeIdentifier = (QCA::Hash("sha1").hashToString(byteArray));

}

quint32 ChordNode::getKeyFromName(QString name){
	QByteArray byteArray; 
	byteArray.append(name);
  QByteArray bytes = QCA::Hash("sha1").hash(byteArray).toByteArray();
	// Create a bit array of the appropriate size
	QBitArray bits(bytes.count()*8);
	// Convert from QByteArray to QBitArray
	for(int i=0; i<bytes.count(); ++i) {
    for(int b=0; b<8; ++b)
      bits.setBit(i*8+b, bytes.at(i)&(1<<(7-b)));
	}
	//Create appropriately sized int
	qint32 x = 0;
	for (uint i=0;i<idlength;++i){
		x<<=1; 			//left shift by one place
		x|=bits[i];	//copy next bit
	}
	//modulo identifier circle size
	return x%(1<<idlength);
}
void ChordNode::sendJoinRequest(QHostAddress host, quint16 port){
	QVariantMap message;
	message.insert(JOIN_KEY, selfNode->key);
	writeToSocket(message,port,host);
}
void ChordNode::writeToSocket(QVariantMap message, quint16 port,QHostAddress host)
{
	QByteArray data = serializeToByteArray(message);
	sock.writeDatagram(data,host,port);	
}

QByteArray ChordNode::serializeToByteArray(QVariantMap message)
{
	QByteArray data;
	QDataStream* stream = new QDataStream(&data, QIODevice::WriteOnly);
	(*stream) << message;
	delete stream;
	return data; 
}

QVariantMap ChordNode::readDeserializeDatagram(QHostAddress* sender, quint16* senderport){
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

void ChordNode::gotNewMessage(){
	//Read Datagram(as a serialized QByteArray) and retrieve sender details
	QHostAddress sender; quint16 port; 
	QVariantMap map = readDeserializeDatagram(&sender,&port);

	if (map.contains(JOIN_KEY))
		handleJoinRequest(map.value(JOIN_KEY).toString(),sender,port);
}

void ChordNode::handleJoinRequest(QString key,QHostAddress sender,quint16 port){

}

void ChordNode::InitializeIdentifierCircle(){
	for (uint i=0; i<idlength;++i){
		finger[i].start = (selfNode->key + (1<<i))% idSpaceSize;
		finger[i].end = (selfNode->key + (1<<(i+1)))% idSpaceSize;
		finger[i].node = selfNode;
	}
	//update successor
	selfNode->successor = finger[1].node;
	//update predecessor
	selfNode->predecessor = selfNode; 
}

Node* ChordNode::findSuccessor(quint32 key){
	Node* n = findPredecessor(key); //Use this to store predecessor
	return n->successor;
}

Node* ChordNode::findPredecessor(quint32 key){
	Node* n = selfNode;
	while(!isIn(key,n->key,n->successor->key))
		n=closestPrecedingFinger(key);
	return n; 
}

Node* ChordNode::closestPrecedingFinger(quint32 key){
	for(int i=CHORD_BITS-1;i>=0;--i){
		if(isIn(finger[i].node->key,selfNode->key,key))
			return finger[i].node;
	}
	return selfNode;
}