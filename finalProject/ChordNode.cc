#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>

#define NEIGHBOR_REQUEST_KEY "Neighbor_Request"
#define NEIGHBOR_REPLY_KEY "Neighbor_Reply"
#define PREDECESSOR_KEY "Predecessor" 
#define SUCCESSOR_KEY "Successor"
#define REQUEST_SOURCE_KEY "Source"
#define	UPDATE_REQUEST_KEY "Update"
#define FINGER_INDEX_KEY "Finger_Index"
#define JOIN -1
#define UPDATEPREDECESSOR -2
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
	// InitializeIdentifierCircle();
	qDebug()<<"I'm"<<selfNode->address<<selfNode->port<<selfNode->key;
	sendJoinRequest(QHostAddress( "128.36.232.20" ),45516);
	printTimer=new QTimer(this);
	connect(printTimer,SIGNAL(timeout()),this,SLOT(printStatus()));
	printTimer->start(5000);

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

	if (map.contains(NEIGHBOR_REQUEST_KEY) && map.contains(REQUEST_SOURCE_KEY)){
		findNeighbors(map.value(NEIGHBOR_REQUEST_KEY).toUInt(),
			map.value(REQUEST_SOURCE_KEY).value<Node>());
	}
	else if (map.contains(NEIGHBOR_REPLY_KEY)){
		handleFoundNeighbor(map,sender,port);
	}
	else if (map.contains(UPDATE_REQUEST_KEY)){
		handleUpdateMessage(map);
	}

}

void ChordNode::SetFingerTableIntervals(){
	for (uint i=0; i<idlength;++i){
		finger[i].start = (selfNode->key + (1<<i))% idSpaceSize;
		finger[i].end = (selfNode->key + (1<<(i+1)))% idSpaceSize;
	}
	InitializeIdentifierCircle();
}
void ChordNode::InitializeIdentifierCircle(){
	for (uint i=0; i<idlength;++i){
		finger[i].node = (*selfNode);
	}
	successor = finger[0].node;
	predecessor = finger[0].node;
}

bool ChordNode::isInInterval(quint32 key,quint32 nodeKey, quint32 succKey, bool leftInc, bool rightInc){
	//check left and right endpoints
	if ((leftInc && key==nodeKey) || (rightInc && key==succKey))
		return true;
	//check interval, enpoints exclusive
	else if (nodeKey>=succKey)
		return (key>succKey); //succKey---nodeKey---key
	else
		return (key>nodeKey && key<succKey); //nodeKey---key---succKey
}

void ChordNode::findNeighbors(quint32 key,Node requestnode){
	sock.delay(5);
	qDebug()<<"finding neighbor"<<key<<"for"<<requestnode.address;
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

void ChordNode::handleFoundNeighbor(QVariantMap map, QHostAddress address, quint32 port){
	if (!map.contains(SUCCESSOR_KEY) || !map.contains(PREDECESSOR_KEY)) {
		qDebug()<<"malformed Neighbor message";
		return;
		}
	quint32 key = map.value(NEIGHBOR_REPLY_KEY).toUInt();
	Node pred = map.value(PREDECESSOR_KEY).value<Node>();
	Node succ = map.value(SUCCESSOR_KEY).value<Node>();
	Node n = Node(address,port,key);
	if (!neighborRequestHash.contains(key))
		return;
	qint32 index = neighborRequestHash.value(key);	
	if (index==JOIN && key==selfNode->key)
		join(pred,succ);
	else if (index<(int)idlength && 
		isInInterval(key,finger[index].start,finger[index].end,true,false)){
			finger[index].node=n;
			// while(index<idlength && isInInterval(finger[index+1].start,selfNode->key,finger[index].node.key,true,false))
			// 	finger[index+1].node=finger[index++].node;
	}
	else if(index==UPDATEPREDECESSOR && updatePredecessorHash.contains(key)){
		sendUpdateMessage(n,(*selfNode),updatePredecessorHash.value(key));
		updatePredecessorHash.remove(key);
	}

	neighborRequestHash.remove(key);
}

void ChordNode::join(Node pred, Node succ){
	qDebug()<<"Join called";
	initFingerTable(pred,succ);
	sock.delay(1); //Allow for some time for change propogation
	// updateOthers();
		
}

void ChordNode::initFingerTable(Node pred, Node succ){
	successor = succ; predecessor = pred;
	//todo: Tell node succ to update predecessor to this node
	sendUpdateMessage(successor,(*selfNode),predecessorIndex);
	//todo: node pred to update succeessor to this node
	sendUpdateMessage(predecessor,(*selfNode),0);

	finger[0].node = succ;
	for(uint i=1;i<idlength;++i){
		neighborRequestHash.insert(finger[i].start,i);
		sendNeighborRequest(joinNode,finger[i].start,(*selfNode));
	}
}

void ChordNode::sendUpdateMessage(Node dest,Node node,quint32 index){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(node);
	message.insert(UPDATE_REQUEST_KEY,qvNode);
	message.insert(FINGER_INDEX_KEY,index);
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::handleUpdateMessage(QVariantMap map){
	if (!map.contains(FINGER_INDEX_KEY))
		return;
	quint32 index = map.value(FINGER_INDEX_KEY).toUInt();
	Node n = map.value(UPDATE_REQUEST_KEY).value<Node>();
	if (index==predecessorIndex){
		if (isInInterval(n.key,predecessor.key,selfNode->key,false,true))
			predecessor=n;
		else
			qDebug()<<"Incorrect Predecessor Update Message";
		return;
	}
	if(index==0 && isInInterval(n.key,selfNode->key,successor.key,true,false)){
		successor = n; finger[0].node=n;
	}
	if(index<idlength && isInInterval(n.key,selfNode->key,finger[index].node.key,true,false)){
		finger[index].node=n;
		sendUpdateMessage(predecessor,n,index);
	}
}

void ChordNode::updateOthers(){
	for(quint32 i=0; i<idlength;++i){
		quint32 prednum = 1<<(i);
		neighborRequestHash.insert(prednum,UPDATEPREDECESSOR);
		updatePredecessorHash.insert(prednum,i);
		findNeighbors((selfNode->key - prednum)%idSpaceSize,(*selfNode));
	}
}
void ChordNode::printStatus(){
	qDebug()<<selfNode->key<<"Successor:"<<successor.address<<successor.port \
	<<"Predecessor"<<predecessor.address<<predecessor.port;
	qDebug()<<"FingerTable";
	for(uint i=0;i<idlength;++i)
		qDebug()<<"["<<finger[i].start<<","<<finger[i].end<<")	:"<<(finger[i].node.address)<<finger[i].node.port; 
}