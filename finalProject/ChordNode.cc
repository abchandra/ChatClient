#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>

#define JOIN_REQUEST_KEY "Join_Request"
#define JOIN_REPLY_KEY "Join_Reply"
#define NEIGHBOR_REQUEST_KEY "Neighbor_Request"
#define NEIGHBOR_REPLY_KEY "Neighbor_Reply"
#define PREDECESSOR_KEY "Predecessor" 
#define FINGER_TABLE_KEY "Finger_Table"
#define SUCCESSOR_KEY "Successor"
#define REQUEST_SOURCE_KEY "Source"

ChordNode::ChordNode(){
	 qRegisterMetaType<Node>();
	 qRegisterMetaTypeStreamOperators<Node>();
	 qRegisterMetaType<FingerTableEntry>();
	 qRegisterMetaTypeStreamOperators<FingerTableEntry>();
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
QDataStream& operator<<(QDataStream &out, const FingerTableEntry &f){
	out<<f.start<<f.end<<f.node;
	return out;
}
QDataStream& operator>>(QDataStream &in, FingerTableEntry &f){
	in>>f.start>>f.end>>f.node;
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
	message.insert(JOIN_REQUEST_KEY, qvNode);
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
	if (map.contains(JOIN_REQUEST_KEY))
		handleJoinRequest(map.value(JOIN_REQUEST_KEY).value<Node>());
	else if (map.contains(NEIGHBOR_REQUEST_KEY) && map.contains(REQUEST_SOURCE_KEY))
		findNeighbors(map.value(NEIGHBOR_REQUEST_KEY).toUInt(),
			map.value(REQUEST_SOURCE_KEY).value<Node>());
	else if (map.contains(NEIGHBOR_REPLY_KEY) && map.contains(PREDECESSOR_KEY)){
		handleFoundNeighbor(map);
	}
	else if(map.contains(JOIN_REPLY_KEY) && map.value(JOIN_REPLY_KEY)==selfNode->key)
		handleJoinReply(map);
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
	neighborRequestHash.insert(key,m);
	findNeighbors(key, requestNode);
}

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

void ChordNode::findNeighbors(quint32 key,Node requestnode){
	if(isInInterval(key,selfNode->key,successor.key,false,true))
		sendNeighborMessage(key,requestnode);
	else
		sendNeighborRequest(closestPrecedingFinger(key),key,requestnode);
}

Node ChordNode::closestPrecedingFinger(quint32 key)
	for(int i=CHORD_BITS-1;i>=0;--i){
		if(isInInterval(finger[i].node.key,selfNode->key,key,false,false))
			return finger[i].node;
	}
	qDebug()<<"Err: Self node is the closest to key";
	return (*selfNode);
}

void ChordNode::sendNeighborRequest(Node dest,quint32 key,Node requestNode){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(requestNode);
	message.insert(NEIGHBOR_REQUEST_KEY,key);
	message.insert(REQUEST_SOURCE_KEY,qvNode);
	// qDebug()<<"Sending Req";
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::sendNeighborMessage(quint32 key, Node requestNode){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(*selfNode);
	message.insert(NEIGHBOR_REPLY_KEY,key);
	message.insert(PREDECESSOR_KEY,qvNode);
	qvNode.setValue(successor);
	message.insert(SUCCESSOR_KEY,qvNode);
	qDebug()<<"sending Predecessor & Successor";
	writeToSocket(message,requestNode.port,requestNode.address);
}

void ChordNode::handleFoundNeighbor(QVariantMap map){
	if (!neighborRequestHash.contains(key))
	return;
	if (!map.contains(SUCCESSOR_KEY)){
		qDebug()<<"malformed Neighbor message";
		return;
		}
	quint32 key = map.value(NEIGHBOR_REPLY_KEY).toUInt();
	Node pred = map.value(PREDECESSOR_KEY).value<Node>();
	Node succ = map.value(SUCCESSOR_KEY).value<Node>();	
	if(neighborRequestHash.value(key)==JOIN){
		qDebug()<<"HAZAA!"<<pred.address<<succ.address;
		sendJoinReply(key,pred,succ);
	}
	neighborRequestHash.remove(key);
}

void ChordNode::sendJoinReply(quint32 key, Node pred, Node succ){
	if (!joinRequestHash.contains(key)){
		qDebug()<<"Unknown node for join";
		return;
	}
	Node njoin = joinRequestHash.value(key);
	QVariantMap message;
	QVariant qvSucc, qvPred;
	qvSucc.setValue(succ); qvPred.setValue(pred);
	message.insert(JOIN_REPLY_KEY,key);
	message.insert(PREDECESSOR_KEY,qvPred);
	message.insert(SUCCESSOR_KEY,qvSucc);
	// Pack the fingerTable
	// QVariantMap fingerMap;	 
	// for(uint i=0;i<idlength;++i){
	// 	QVariant qvFinger; qvFinger.setValue(finger[i]);
	// 	fingerMap.insert(QString::number(i),qvFinger);
	// }
	// message.insert(FINGER_TABLE_KEY,fingerMap);
	// joinRequestHash.remove(key);
	// writeToSocket(message,njoin.port,njoin.address);

}

void ChordNode::handleJoinReply(QVariantMap map){
	if (!map.contains(PREDECESSOR_KEY) ||	
			!map.contains(SUCCESSOR_KEY)){
		qDebug()<<"malformed join Reply. Try Again";
		return;
	}
	Node pred = map.value(PREDECESSOR_KEY).value<Node>();
	Node succ = map.value(SUCCESSOR_KEY).value<Node>();
	QVariantMap fingerMap = map.value(FINGER_TABLE_KEY);
	FingerTableEntry rcvdFingerTable[idlength];
	for(uint i=0;i<idlength;++i){
		if(!fingerMap.contains(i.toString())){
			rcvdFingerTable[i] = (*selfNode);
			continue;
		}
		rcvdFingerTable[i] = fingerMap.value(i.toString()).value<FingerTableEntry>();
	}
	join(pred,succ,rcvdFingerTable);
}
void ChordNode::join(Node pred, Node succ, FingerTableEntry rcvdFinger[idlength]){
	initFingerTable(pred,succ,rcvdFinger)
}

void ChordNode::initFingerTable(Node pred, Node succ, FingerTableEntry rcvdFinger[idlength]){
	successor = succ; predecessor = pred;
	//todo: Tell node succ to update predecessor to this node
	//todo: node pred to update succeessor to this node
	finger[0].node = succ;
	for(uint i=0;i<idlength-1;++i){
		if(isInInterval(finger[i+1].start,selfNode->key,finger[i].node,true,false))
			finger[i+1].node = finger[i].node;
		else
			sendNeighborRequest(,key,requestnode);
	}

}

