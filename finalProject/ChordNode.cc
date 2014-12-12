#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>

#define JOIN_KEY "Join"
#define PREDECESSOR_REQUEST_KEY "Predecessor_Request"
#define PREDECESSOR_REPLY_KEY "Predecessor_Reply"
#define PREDECESSOR_KEY "Predecessor" 
#define SUCCESSOR_KEY "Successor"
#define REQUEST_SOURCE_KEY "Source"

ChordNode::ChordNode(){
	 qRegisterMetaType<Node>();
	  qRegisterMetaTypeStreamOperators<Node>();
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
	//initialize selfNode
	setCreateSelfNode();
	//initialize fingerTable with intervals
	finger = new FingerTableEntry[idlength];
	SetFingerTableIntervals();
	//Register a callback on NetSocket's readyRead signal to
	//read pending datagrams.
	connect(&sock, SIGNAL(readyRead()),this, SLOT(gotNewMessage()));
	InitializeIdentifierCircle();
	qDebug()<<"I'm"<<selfNode->address<<selfNode->port<<selfNode->key;
	// sendJoinRequest(QHostAddress( "128.36.232.35" ),45516);

	//Dialog to connect to a chord node

	//Here we need preserve join conditions from 4.4
	//For now:
	//	locate succ and pred of succ
	//	add self to between
	//	switch keys around
	//	Will need a stabilization state
	//	
}
QDataStream& operator<<(QDataStream &out, const Node &n){
	out<<n.key<<n.address<<n.port;
	return out;
}
QDataStream& operator>>(QDataStream &in, Node &n){
	in>>n.key>>n.address>>n.port;
	return in;
}

void ChordNode::setCreateSelfNode(){
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
	QVariant qvNode; qvNode.setValue(*selfNode);
	message.insert(JOIN_KEY, qvNode);
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
	qDebug()<<"PING from:"<<sender<<port;
	if (map.contains(JOIN_KEY))
		handleJoinRequest(map.value(JOIN_KEY).value<Node>());
	else if (map.contains(PREDECESSOR_REQUEST_KEY) && map.contains(REQUEST_SOURCE_KEY))
		findPredecessor(map.value(PREDECESSOR_REQUEST_KEY).toUInt(),
			map.value(REQUEST_SOURCE_KEY).value<Node>());
	else if (map.contains(PREDECESSOR_REPLY_KEY) && map.contains(PREDECESSOR_KEY)){
		handleFoundPredecessor(map);
	}
}

void ChordNode::handleJoinRequest(Node njoin){
		//Add node to hash map
	qDebug()<<"Join Req"<<njoin.address<<njoin.port<<njoin.key;
		joinRequestHash.insert(njoin.key,njoin);
		findSuccessor(njoin.key,*selfNode,JOIN);
	}
void ChordNode::SetFingerTableIntervals(){
	for (uint i=0; i<idlength;++i){
		finger[i].start = (selfNode->key + (1<<i))% idSpaceSize;
		finger[i].end = (selfNode->key + (1<<(i+1)))% idSpaceSize;
		// qDebug()<<"finger"<<i<<finger[i].start;
	}
}
void ChordNode::InitializeIdentifierCircle(){
	for (uint i=0; i<idlength;++i){
		finger[i].node = (*selfNode);
	}
	successor = finger[0].node;
	predecessor = finger[0].node;
}
//First we need to contact other nodes to find predecessor
void ChordNode::findSuccessor(quint32 key,Node requestNode,Mode m){
	predecessorRequestHash.insert(key,m);
	findPredecessor(key, requestNode);
}
//if the predecessor has been found, return the successor
//  ChordNode::findSuccessor(quint32 key, Node requestnode, Node predecessor){

// }
bool ChordNode::isInInterval(quint32 key,quint32 nodeKey, quint32 succKey, bool leftInc, bool rightInc){
	//check left and right endpoints
	if ((leftInc && key==nodeKey) || (rightInc && key==succKey))
		return true;
	//check interval, enpoints exclusive
	else if (nodeKey>=succKey)
		return (key>nodeKey); //succKey---nodeKey---key
	else
		return (key>nodeKey && key<succKey); //nodeKey---key---succKey
}
void ChordNode::findPredecessor(quint32 key,Node requestnode){
	if(isInInterval(key,selfNode->key,successor.key,false,true))
		sendPredecessorMessage(key,requestnode);
	else
		sendPredecessorRequest(closestPrecedingFinger(key),key,requestnode);
}

Node ChordNode::closestPrecedingFinger(quint32 key){
	for(int i=CHORD_BITS-1;i>=0;--i){
		if(isInInterval(finger[i].node.key,selfNode->key,key,false,false))
			return finger[i].node;
	}
	qDebug()<<"Err: Self node is the closest to key";
	return (*selfNode);
}

void ChordNode::sendPredecessorRequest(Node dest,quint32 key,Node requestNode){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(requestNode);
	message.insert(PREDECESSOR_REQUEST_KEY,key);
	message.insert(REQUEST_SOURCE_KEY,qvNode);
	// qDebug()<<"Sending Req";
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::sendPredecessorMessage(quint32 key, Node requestNode){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(*selfNode);
	message.insert(PREDECESSOR_REPLY_KEY,key);
	message.insert(PREDECESSOR_KEY,qvNode);
	qvNode.setValue(successor);
	message.insert(SUCCESSOR_KEY,qvNode);
	qDebug()<<"sending Predecessor";
	writeToSocket(message,requestNode.port,requestNode.address);
}

void ChordNode::handleFoundPredecessor(QVariantMap map){
	quint32 key = map.value(PREDECESSOR_REPLY_KEY).toUInt();
	Node pred = map.value(PREDECESSOR_KEY).value<Node>();
	if (!predecessorRequestHash.contains(key))
		return;
	if(predecessorRequestHash.value(key)==JOIN){
		Node succ = map.value(SUCCESSOR_KEY).value<Node>();
		qDebug()<<"HAZAA!"<<pred.address<<succ.address;
	}
			;// join(key,pred,map.value(SUCCESSOR_KEY));
		// else if (map.contains)
}



















// Node ChordNode::findSuccessor(quint32 key){
// 	Node n = findPredecessor(key); //Use this to store predecessor
// 	return n->successor;
// }

// Node ChordNode::findPredecessor(quint32 key){
// 	Node n = selfNode;
// 	while(!isIn(key,n->key,n->successor->key))
// 		n=closestPrecedingFinger(key);
// 	return n; 
// }

// Node ChordNode::closestPrecedingFinger(quint32 key){
// 	for(int i=CHORD_BITS-1;i>=0;--i){
// 		if(isIn(finger[i].node->key,selfNode->key,key))
// 			return finger[i].node;
// 	}
// 	return selfNode;
// }

// void ChordNode::joinNode(Node njoin){
// 	//catch error
// 	initFingerTable(njoin);

// }

// void ChordNode::initFingerTable(Node njoin){
// 	finger[0].node = njoin.findSuccessor
// }