#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>

#define NEIGHBOR_REQUEST_KEY "Neighbor_Request"
#define NEIGHBOR_REPLY_KEY "Neighbor_Reply"
#define PREDECESSOR_KEY "Predecessor" 
#define FINGER_TABLE_KEY "Finger_Table"
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
	neighborRequestHash.insert(selfNode->key,JOIN);
	joinNode = Node(host,port,0);
	sendNeighborRequest(joinNode,selfNode->key,(*selfNode));
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

	if (map.contains(NEIGHBOR_REQUEST_KEY) && map.contains(REQUEST_SOURCE_KEY))
		findNeighbors(map.value(NEIGHBOR_REQUEST_KEY).toUInt(),
			map.value(REQUEST_SOURCE_KEY).value<Node>());
	else if (map.contains(NEIGHBOR_REPLY_KEY)){
		handleFoundNeighbor(map);
	}

}

void ChordNode::SetFingerTableIntervals(){
	for (uint i=0; i<idlength;++i){
		finger[i].start = (selfNode->key + (1<<i))% idSpaceSize;
		finger[i].end = (selfNode->key + (1<<(i+1)))% idSpaceSize;
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

Node ChordNode::closestPrecedingFinger(quint32 key){
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
	if (!map.contains(SUCCESSOR_KEY) || !map.contains(PREDECESSOR_KEY)) {
		qDebug()<<"malformed Neighbor message";
		return;
		}
	quint32 key = map.value(NEIGHBOR_REPLY_KEY).toUInt();
	Node pred = map.value(PREDECESSOR_KEY).value<Node>();
	Node succ = map.value(SUCCESSOR_KEY).value<Node>();
	if (!neighborRequestHash.contains(key))
		return;	
	if (neighborRequestHash.value(key)==JOIN){
		if (key==selfNode->key)
			join(pred,succ);
	}
	neighborRequestHash.remove(key);
}

void ChordNode::join(Node pred, Node succ){
	initFingerTable(pred,succ);
}

void ChordNode::initFingerTable(Node pred, Node succ){
	successor = succ; predecessor = pred;
	//todo: Tell node succ to update predecessor to this node
	//todo: node pred to update succeessor to this node
	finger[0].node = succ;
	for(uint i=0;i<idlength-1;++i){
		if(isInInterval(finger[i+1].start,selfNode->key,finger[i].node.key,true,false))
			finger[i+1].node = finger[i].node;
		else
			sendNeighborRequest(joinNode,finger[i+1].start,(*selfNode));
	}

}

