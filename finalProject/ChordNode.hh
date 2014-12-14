/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/

#ifndef CHORDSTER_MAIN_HH
#define CHORDSTER_MAIN_HH
#include "NetSocket.hh"
#include <QComboBox>
#include <QtCrypto>
#include <QDataStream>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHash>
#include <QHostInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QList>
#include <QMap>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QUdpSocket>
#include <QVBoxLayout>
#include <QListWidget>

#define CHORD_BITS 16
class Node
{
public:
	Node(){};
	~Node(){};
	Node(const Node &n){
		address = n.address;port = n.port; key = n.key;
	}

	Node(QHostAddress a,quint16 p,quint32 k){
		address = a; port = p; key = k;
	}
	QHostAddress address;
	quint16 port;
	quint32 key;
};

class FingerTableEntry
{
public:
	//Refer Table 1 of Chord paper
	quint32 start;	//inclusive
	quint32 end;		//exclusive
	Node node;
};
Q_DECLARE_METATYPE(Node);
//Main class for chordster instance
class ChordNode : public QDialog
{
	Q_OBJECT

public:
	ChordNode();

public slots:
	void gotNewMessage();					//Handles readyRead() for incoming datagrams
	void printStatus();
private:
	static const quint32 idlength = CHORD_BITS;
	static const quint32 idSpaceSize = 1<<idlength;
	static const quint32 predecessorIndex = idlength+1;
	

	QVBoxLayout *layout1;					//Layout of GUI
	QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
	QPushButton* downloadfilebutton;//Launch dialog for file download	


	NetSocket sock;								//Netsocket instance to bind to a port
	Node* selfNode;
	Node joinNode;
	FingerTableEntry* finger;
	Node predecessor;
	Node successor;
	QTimer* printTimer;
	QHash<quint32,qint32> neighborRequestHash;
	QHash<quint32,quint32> updatePredecessorHash;
	void InitializeIdentifierCircle();//Init a new id circle
	quint32 getKeyFromName(QString);	//Create chord key from given QString		
	void setCreateSelfNode();			//Create and store the chord key for node
	void sendJoinRequest(QHostAddress, quint16); // Send request to join chord
	void writeToSocket(QVariantMap, quint16,QHostAddress);//Calls writeDatagram
	QByteArray serializeToByteArray(QVariantMap);	//Serialze QVMap to QByteArray
	QVariantMap readDeserializeDatagram(QHostAddress*, quint16*);
	void SetFingerTableIntervals();
	void findNeighbors(quint32, Node);
	Node closestPrecedingFinger(quint32);
	void sendNeighborRequest(Node,quint32,Node);
	void sendNeighborMessage(quint32,Node);
	void handleFoundNeighbor(QVariantMap,QHostAddress,quint32);
	bool isInInterval(quint32,quint32,quint32,bool,bool);
	void join(Node,Node);
	void initFingerTable(Node,Node);
	void sendUpdateMessage(Node,Node,quint32);
	void handleUpdateMessage(QVariantMap);
	void updateOthers();
};

#endif // CHORDSTER_MAIN_HH
