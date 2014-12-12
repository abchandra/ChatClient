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
	FingerTableEntry(){};
	FingerTableEntry(const FingerTableEntry &f){
		start = f.start; end = f.end;
		node = f.node;
	};
	~FingerTableEntry(){};
	//Refer Table 1 of Chord paper
	quint32 start;	//inclusive
	quint32 end;		//exclusive
	Node node;
};
Q_DECLARE_METATYPE(FingerTableEntry);
Q_DECLARE_METATYPE(FingerTableEntry*);
Q_DECLARE_METATYPE(Node);
//Main class for chordster instance
class ChordNode : public QDialog
{
	Q_OBJECT

public:
	ChordNode();

public slots:
	void gotNewMessage();					//Handles readyRead() for incoming datagrams
	
private:
	static const quint32 idlength = CHORD_BITS;
	static const quint32 idSpaceSize = 1<<idlength;
	

	QVBoxLayout *layout1;					//Layout of GUI
	QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
	QPushButton* downloadfilebutton;//Launch dialog for file download	


	NetSocket sock;								//Netsocket instance to bind to a port
	Node* selfNode;
	FingerTableEntry* finger;
	Node predecessor;
	Node successor;
	
	enum Mode{PRED, SUCC, JOIN};
	QHash<quint32,Mode> predecessorRequestHash;
	QHash<quint32,Node> joinRequestHash;
	void InitializeIdentifierCircle();//Init a new id circle
	quint32 getKeyFromName(QString);	//Create chord key from given QString		
	void setCreateSelfNode();			//Create and store the chord key for node
	void sendJoinRequest(QHostAddress, quint16); // Send request to join chord
	void writeToSocket(QVariantMap, quint16,QHostAddress);//Calls writeDatagram
	QByteArray serializeToByteArray(QVariantMap);	//Serialze QVMap to QByteArray
	QVariantMap readDeserializeDatagram(QHostAddress*, quint16*);
	void handleJoinRequest(Node);
	void SetFingerTableIntervals();
	void findSuccessor(quint32, Node, Mode m=SUCC);
	void findPredecessor(quint32, Node);
	Node closestPrecedingFinger(quint32);
	void sendPredecessorRequest(Node,quint32,Node);
	void sendPredecessorMessage(quint32,Node);
	void handleFoundPredecessor(QVariantMap);
	bool isInInterval(quint32,quint32,quint32,bool,bool);
};

#endif // CHORDSTER_MAIN_HH
