/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#ifndef CHORDSTER_MAIN_HH
#define CHORDSTER_MAIN_HH
#include "NetSocket.hh"
#include "FileShareManager.hh"
#include "DownloadFileDialog.hh"
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

#define CHORD_BITS 15
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
	ChordNode(bool x);
	bool hasjoinhost;

public slots:
	void gotNewMessage();					//Handles readyRead() for incoming datagrams
	void handleShareFileButton();	//Handles released() for file sharing button
	void handleDownloadFile(quint32 key);
	void handleDownloadButton();	//Generate dialog for requesting a download
	void handleStabilityTimeout();
	void handleshowfingerTable();
	void printStatus();
	void handlehostEntered(QString);
private:
	static const quint32 idlength = CHORD_BITS;
	static const quint32 idSpaceSize = 1<<idlength;
	static const quint32 predecessorIndex = idlength+1;
	

	QVBoxLayout *layout1;					//Layout of GUI
	QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
	QPushButton* downloadfilebutton;//Launch dialog for file download	
	QPushButton* showFingerTablebutton;
	DownloadFileDialog* sharedialog;
	ConnectDialog* connectdialog;
	QFileDialog* filedialog;
	QLineEdit* hostline;

	NetSocket sock;								//Netsocket instance to bind to a port
	FileShareManager filemanager;
	
	Node* selfNode;
	Node joinNode;
	FingerTableEntry* finger;
	Node predecessor;
	Node successor;
	QTimer* stabilityTimer;
	QTimer* printTimer;
	QHash<quint32,qint32> neighborRequestHash;
	QHash<quint32,quint32> updatePredecessorHash;
	QMap<quint32,Node>	KeyLocationHash;
	QHash<quint32,QByteArray> myUploadHash;
	QMap<QByteArray,quint32> requestedMetaFiles;
	bool hasjoined;
	void InitializeIdentifierCircle();//Init a new id circle
	quint32 getKeyFromName(QString);	//Create chord key from given QString		
	quint32 getKeyFromByteArray(QByteArray);
	void setCreateSelfNode();			//Create and store the chord key for node
	void sendJoinRequest(QHostAddress, quint16); // Send request to join chord
	void writeToSocket(QVariantMap, quint16,QHostAddress);//Calls writeDatagram
	QByteArray serializeToByteArray(QVariantMap);	//Serialze QVMap to QByteArray
	QVariantMap readDeserializeDatagram(QHostAddress*, quint16*);
	void SetFingerTableIntervals();
	void findNeighbors(quint32, Node);
	Node closestPrecedingFinger(quint32);
	void sendNeighborRequest(Node,quint32,Node);
	void sendNeighborMessage(quint32,Node,bool isGuess=false);
	void handleFoundNeighbor(QVariantMap,QHostAddress,quint32);
	bool isInInterval(quint32,quint32,quint32,bool,bool);
	void join(Node,Node);
	void initFingerTable(Node,Node);
	void sendUpdateMessage(Node,Node,quint32);
	void handleUpdateMessage(QVariantMap);
	void sendKeyMessage(quint32,Node,Node);
	void handleKeyMessage(QVariantMap);
	void sendDownloadRequest(quint32,Node,Node);
	void handleDownloadRequest(QVariantMap);
	void sendDownloadReply(quint32,QByteArray,Node,Node);
	void handleDownloadReply(QVariantMap);
	void sendBlockRequestMessage(Node,QByteArray);
	void handleBlockRequestMessage(QVariantMap);
	void sendBlockReplyMessage(QByteArray,QByteArray,Node);
	void handleBlockReplyMessage(QVariantMap);
	void sendNotifyMessage(Node);
	void handleNotifyMessage(QVariantMap);
	void updateOthers();
	void addFilesToChord(QList<QByteArray>);
};

#endif // CHORDSTER_MAIN_HH
