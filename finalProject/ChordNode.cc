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
#define NEW_UPLOAD_KEY "New_Upload"
#define DOWNLOAD_KEY "Download_Request"
#define DOWNLOAD_REPLY_KEY "Download_Reply"
#define BLOCK_REQUEST_KEY "BlockRequest"
#define BLOCK_REPLY_KEY "BlockReply"
#define DATA_KEY "Data"
#define STABILITY_KEY "Stability"

#define JOIN -1
#define UPDATEPREDECESSOR -2
#define SENDKEY -3
#define DOWNLOAD -4
#define STABILITY -5
ChordNode::ChordNode(){
	qRegisterMetaType<Node>();
	qRegisterMetaTypeStreamOperators<Node>();
	setWindowTitle("Chordster");
	hasjoined = false;
	sharefilebutton = new QPushButton("Share file...",this);
	downloadfilebutton = new QPushButton("Download file...",this);

	//Make buttons not clickable on return Press
	sharefilebutton->setAutoDefault(false);
	downloadfilebutton->setAutoDefault(false);

	//Register callback on file share button to open file sharing dialog
	connect(sharefilebutton,SIGNAL(released()),this,SLOT(handleShareFileButton()));
	//Register callback on download file button to open file sharing dialog
	connect(downloadfilebutton,SIGNAL(released()),this,SLOT(handleDownloadButton()));
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
	sendJoinRequest(QHostAddress( "128.36.232.41" ),45516);
	printTimer=new QTimer(this);
	connect(printTimer,SIGNAL(timeout()),this,SLOT(printStatus()));
	printTimer->start(5000);


	stabilityTimer=new QTimer(this);
	connect(stabilityTimer,SIGNAL(timeout()),this,SLOT(handleStabilityTimeout()));
	stabilityTimer->start(10000);
}
QDataStream& operator<<(QDataStream &out, const Node &n){
	out<<n.key<<n.address<<n.port;
	return out;
}
QDataStream& operator>>(QDataStream &in, Node &n){
	in>>n.key>>n.address>>n.port;
	return in;
}

void ChordNode::handleStabilityTimeout(){
	neighborRequestHash.insert(successor.key,STABILITY);
	findNeighbors(successor.key,(*selfNode));
	int i=0;
	while(!i){
		qsrand(qrand());
		i = qrand()%idlength;
	}
	neighborRequestHash.insert(finger[i].start,i);
	findNeighbors(finger[i].start,(*selfNode));
	if (KeyLocationHash.isEmpty() || predecessor.key==selfNode->key)
		return;
	QMap<quint32,Node>::iterator it = KeyLocationHash.begin();
	while(it!=KeyLocationHash.end()){
		if (!isInInterval(it.key(),predecessor.key,selfNode->key,false,false)){
			//move Keys around
			neighborRequestHash.insert(it.key(),SENDKEY);
			findNeighbors(it.key(),(*selfNode));
		}
		++it;
	}
}
void ChordNode::handleShareFileButton() {
	filedialog = new QFileDialog(this);
	filedialog->setFileMode(QFileDialog::ExistingFiles);
	QStringList filenames;
	if (filedialog->exec())
	  filenames = filedialog->selectedFiles();
	//Call FileShareManager instance to handle files
	addFilesToChord(filemanager.addFiles(filenames));
	delete filedialog;
	qDebug()<<filenames;

}

void ChordNode::handleDownloadButton() {
	sharedialog = new DownloadFileDialog();
	connect(sharedialog,SIGNAL(downloadfile(quint32)),this,
		SLOT(handleDownloadFile(quint32)));
	sharedialog->show();
}

void ChordNode::handleDownloadFile(quint32 key) {
	//Close the dialog and free it
	sharedialog->close();
	delete sharedialog;
	qDebug()<<"Making request for"<<key;
	neighborRequestHash.insert(key,DOWNLOAD);
	findNeighbors(key,(*selfNode));
	// //update requested metafiles
	// requestedmetafiles.insert(QByteArray::fromHex(fileid),qMakePair(destination,QString("")));
	// sendBlockRequestMessage(destination,QByteArray::fromHex(fileid));
}

void ChordNode::addFilesToChord(QList<QByteArray> addedFilesHashList){
	QList<QByteArray>::iterator i;
	for(i=addedFilesHashList.begin();i!=addedFilesHashList.end();++i){
		quint32 key = getKeyFromByteArray(*i);
		//store in upload hash
		myUploadHash.insert(key,(*i));
		//send to successor of key
		neighborRequestHash.insert(key,SENDKEY);
		findNeighbors(key,(*selfNode));
	}
}

void ChordNode::setCreateSelfNode(){
 	if (!sock.hostInfo)
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
  return getKeyFromByteArray(bytes);
}
quint32 ChordNode::getKeyFromByteArray(QByteArray bytes){
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
	if (host==selfNode->address && port==selfNode->port)
		return;
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
	else if (map.contains(NEW_UPLOAD_KEY) && map.contains(REQUEST_SOURCE_KEY)){
		handleKeyMessage(map);
	}
	else if (map.contains(DOWNLOAD_KEY) && map.contains(REQUEST_SOURCE_KEY)){
		handleDownloadRequest(map);
	}
	else if (map.contains(DOWNLOAD_REPLY_KEY) && map.contains(REQUEST_SOURCE_KEY)){
		handleDownloadReply(map);
	}
	else if(map.contains(BLOCK_REQUEST_KEY)){
		handleBlockRequestMessage(map);
	}
	else if(map.contains(BLOCK_REPLY_KEY) && map.contains(DATA_KEY)){
		handleBlockReplyMessage(map);
	}
	else if(map.contains(STABILITY_KEY)){
		handleNotifyMessage(map);
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
	if (nodeKey==succKey)
		return true;
	//check left and right endpoints
	if ((leftInc && key==nodeKey) || (rightInc && key==succKey))
		return true;
	//check interval, enpoints exclusive
	else if (nodeKey>succKey)
		return ((key>nodeKey && key>succKey) ||	  //succKey---nodeKey---key
						(key<nodeKey && key<succKey));		//key---succKey---nodeKey
	else
		return (key>nodeKey && key<succKey); //nodeKey---key---succKey
}

void ChordNode::findNeighbors(quint32 key,Node requestnode){
	qDebug()<<"finding neighbor"<<key<<"for"<<requestnode.address;
	if (isInInterval(key,selfNode->key,successor.key,false,true))
		sendNeighborMessage(key,requestnode);
	else{
		Node n= closestPrecedingFinger(key);
		if (n.key!=selfNode->key){
			qDebug()<<"Asking"<<n.key;
			sendNeighborRequest(n,key,requestnode);
		}
		else {
			//Warning: Dicey patch to apparent oversight in paper pseudocode
			//This case deals with finger tables being unstable to such an
			//extent that the findneighbor operation cannot resolve the 
			//neighbors correctly. This is very true for the first node
			//or two to join. Hopefully, we will implement stability to
			//fix this up anyway.
			qDebug()<<"Sending self as succ/pred to break tie";
			sendNeighborMessage(key,requestnode,true);		
		}
	}
}

Node ChordNode::closestPrecedingFinger(quint32 key){
	for(int i=CHORD_BITS-1;i>=0;--i){
		if (isInInterval(finger[i].node.key,selfNode->key,key,false,false))
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

void ChordNode::sendNeighborMessage(quint32 key, Node requestNode,bool isGuess){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(*selfNode);
	message.insert(NEIGHBOR_REPLY_KEY,key);
	message.insert(PREDECESSOR_KEY,qvNode);
	if (!isGuess)
		qvNode.setValue(successor);
	message.insert(SUCCESSOR_KEY,qvNode);

	qDebug()<<"sending myself neighbor"<<key<<"for"<<requestNode.key;
	writeToSocket(message,requestNode.port,requestNode.address);
}
void ChordNode::sendKeyMessage(quint32 key, Node dest, Node owner){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(owner);
	message.insert(NEW_UPLOAD_KEY,key);
	message.insert(REQUEST_SOURCE_KEY,qvNode);
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::handleKeyMessage(QVariantMap map){
	quint32 key= map.value(NEW_UPLOAD_KEY).toUInt();
	Node n= map.value(REQUEST_SOURCE_KEY).value<Node>();
	qDebug()<<"Got new upload at"<<key<<"by node at"<<n.key;
	KeyLocationHash.insert(key,n);
}

void ChordNode::sendDownloadRequest(quint32 key, Node dest, Node requestNode){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(requestNode);
	message.insert(DOWNLOAD_KEY,key);
	message.insert(REQUEST_SOURCE_KEY,qvNode);
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::handleDownloadRequest(QVariantMap map){
	quint32 key= map.value(DOWNLOAD_KEY).toUInt();
	Node n= map.value(REQUEST_SOURCE_KEY).value<Node>();
	if(myUploadHash.contains(key)){
		qDebug()<<"READY FOR THE BLOCKLIST TRANSACTIONS TO BEGIN";
		QByteArray requestedMetaFile= myUploadHash.value(key);
		sendDownloadReply(key,requestedMetaFile,(*selfNode),n);		
		return;
	}
	if (!KeyLocationHash.contains(key))
		return;
	Node uploaderNode = KeyLocationHash.value(key);
	qDebug()<<"Got a download request for"<<key<<"by node at"<<n.key;
	sendDownloadRequest(key,uploaderNode,n);
}

void ChordNode::sendDownloadReply(quint32 key,QByteArray filehash,Node from, Node dest){
	QVariantMap message;
	QVariant qvNode; qvNode.setValue(from);
	message.insert(DOWNLOAD_REPLY_KEY,key);
	message.insert(REQUEST_SOURCE_KEY,qvNode);
	message.insert(DATA_KEY,filehash);
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::handleDownloadReply(QVariantMap map){
	if(!map.contains(DATA_KEY))
		return;
	quint32 key= map.value(DOWNLOAD_REPLY_KEY).toUInt();
	Node source= map.value(REQUEST_SOURCE_KEY).value<Node>();
	QByteArray metafilehash=map.value(DATA_KEY).toByteArray();
	//sanity check
	if (key!=getKeyFromByteArray(metafilehash))
		return;
	//Now we can add this hash to our requested metafiles
	requestedMetaFiles.insert(metafilehash,key);
	sendBlockRequestMessage(source,metafilehash);
}

void ChordNode::sendBlockRequestMessage(Node dest, QByteArray hashval){
	QVariantMap map;
	QVariant qvNode; qvNode.setValue(*selfNode);
	map.insert(BLOCK_REQUEST_KEY,hashval);
	map.insert(REQUEST_SOURCE_KEY,qvNode);
	writeToSocket(map,dest.port,dest.address);

}

void ChordNode::handleBlockRequestMessage(QVariantMap map){
	QByteArray hashval = map.value(BLOCK_REQUEST_KEY).toByteArray();
	Node source= map.value(REQUEST_SOURCE_KEY).value<Node>();

	QByteArray* result = filemanager.findBlockFromHash(hashval);
	if (result==NULL)
		qDebug()<<"Err: block not found";
	if(result!=NULL && filemanager.sanityCheck(hashval,(*result))){
		sendBlockReplyMessage(*result,hashval,source);
	}
}

void ChordNode::sendBlockReplyMessage(QByteArray block,QByteArray hashval, Node dest){
	QVariantMap map;
	QVariant qvNode; qvNode.setValue(*selfNode);
	map.insert(DATA_KEY,block);
	map.insert(BLOCK_REPLY_KEY,hashval);
	map.insert(REQUEST_SOURCE_KEY,qvNode);
	writeToSocket(map,dest.port,dest.address);
}

void ChordNode::handleBlockReplyMessage(QVariantMap map){
	QByteArray data = map.value(DATA_KEY).toByteArray();
	QByteArray hashval = map.value(BLOCK_REPLY_KEY).toByteArray();
	Node source= map.value(REQUEST_SOURCE_KEY).value<Node>();
	if (filemanager.sanityCheck(hashval,data)){
		QByteArray nexthash="";
		if (requestedMetaFiles.contains(hashval)) {
			QString filename = QString::number(requestedMetaFiles.value(hashval));
			requestedMetaFiles.remove(hashval);
		  nexthash = filemanager.addDownload(hashval,data,source.key,filename);
		}
		else{
			nexthash = filemanager.addBlock(hashval,data,source.key);
		}
		if (!nexthash.isEmpty()){
			sendBlockRequestMessage(source,nexthash);
		}
	}
}
void ChordNode::sendNotifyMessage(Node dest){
	QVariantMap message; 
	QVariant qvNode; qvNode.setValue(*selfNode);
	message.insert(STABILITY_KEY,qvNode);
	writeToSocket(message,dest.port,dest.address);
}

void ChordNode::handleNotifyMessage(QVariantMap map){
	Node x = map.value(STABILITY_KEY).value<Node>();
	if(predecessor.key!=x.key && isInInterval(x.key,predecessor.key,selfNode->key,false,false))
		predecessor=x;
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
	if (!neighborRequestHash.contains(key)){
		return;
	}
	qint32 index = neighborRequestHash.value(key);	
	if (index==JOIN && key==selfNode->key)
		join(pred,succ);
	else if (index==UPDATEPREDECESSOR && updatePredecessorHash.contains(key)){
		qDebug()<<"Asking"<<pred.key<<"to update at"<<updatePredecessorHash.value(key)<<"if needed";
		sendUpdateMessage(pred,(*selfNode),updatePredecessorHash.value(key));
		updatePredecessorHash.remove(key);
	}
	else if (index==SENDKEY){
		if (KeyLocationHash.contains(key)){
			sendKeyMessage(key,succ,KeyLocationHash.value(key));
			KeyLocationHash.remove(key);
		}
		else {
			sendKeyMessage(key,succ,(*selfNode));
		}
	}
	else if(index==DOWNLOAD){
	qDebug()<<"Sending download request for"<<key<<"to"<<succ.key;
		sendDownloadRequest(key,succ,(*selfNode));
	}
	else if (index==STABILITY){
		Node x = pred; 
		if (x.key != selfNode->key && isInInterval(x.key,selfNode->key,key,false,false)){
			successor = x; finger[0].node=x;
		}
		sendNotifyMessage(successor);
	}
	else if (index>0 && index<(int)idlength && finger[index].start==key){ 
		qDebug()<<"My finger"<<index<<"is"<<succ.key;
		finger[index].node=succ;
		if (!hasjoined && neighborRequestHash.size()<=1){
			neighborRequestHash.remove(key);
			hasjoined=true;
			updateOthers(); 
			return;
		}
			// while(index<(int)idlength 
			// 	&& isInInterval(finger[index+1].start,selfNode->key,finger[index].node.key,true,false))
			// 	finger[index+1].node=finger[index++].node;
	}

	neighborRequestHash.remove(key);
}

void ChordNode::join(Node pred, Node succ){
	qDebug()<<"Join called";
	initFingerTable(pred,succ);		
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
	qDebug()<<"Update Message Received:"<<"["<<finger[index].start<<","<<finger[index].end<<")	:"<<"current:"<<
	finger[index].node.key<<"suggested:"<<n.key;

	if (index==0 && isInInterval(n.key,selfNode->key,successor.key,true,false)){
		successor = n; finger[0].node=n;
	}
	if (index<idlength && isInInterval(n.key,selfNode->key,finger[index].node.key,true,false)){
		
		if (n.key==selfNode->key) //Message updates gone full cycle
			return;
		qDebug()<<"Accepted.";
		finger[index].node=n;
		sendUpdateMessage(predecessor,n,index);
	}
}

void ChordNode::updateOthers(){
	for(quint32 i=0; i<idlength;++i){
		quint32 key = (selfNode->key - (1<<i))%idSpaceSize;
		neighborRequestHash.insert(key,UPDATEPREDECESSOR);
		updatePredecessorHash.insert(key,i);
		findNeighbors(key,(*selfNode));
	}
}
void ChordNode::printStatus(){
	qDebug()<<selfNode->key<<"Successor:"<<successor.address<<successor.port \
	<<"Predecessor"<<predecessor.address<<predecessor.port;
	qDebug()<<"FingerTable";
	for(uint i=0;i<idlength;++i)
		qDebug()<<"["<<finger[i].start<<","<<finger[i].end<<")	:"<<finger[i].node.key<<(finger[i].node.address)<<finger[i].node.port; 
}