#include "ChordNode.hh"
#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QBitArray>
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
  nodeKey = getKeyFromName(nodeName);
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
	for (int i=0;i<idlength;++i){
		x<<=1; 			//left shift by one place
		x|=bits[i];	//copy next bit
	}
	//modulo identifier circle size
	return x%(1<<idlength);
}