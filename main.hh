/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/

#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH


#include <QComboBox>
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
#include "FileShareManager.hh"
//Text Edit for chat line
//TODO: 2-3 line bug. Consider QtPlainTextEdit
class CustomTextEdit : public QTextEdit
{
	Q_OBJECT
	public:
		CustomTextEdit(QWidget* parent = 0): QTextEdit(parent = 0){} 
	protected:
		void keyPressEvent(QKeyEvent* event);
	signals:
		void returnPressed();
};

class DownloadFileDialog : public QDialog
{
	Q_OBJECT
public:
	DownloadFileDialog(QStringList);

private:
	QVBoxLayout *layout;
	QComboBox *destinationbox;
	CustomTextEdit *fileidtext;
	QPushButton* reqbutton;
	QString destinationorigin;

public slots:
	void handleRequestButton();
	void handleSelectionChanged(int);
signals:
	void downloadfile(QString,QByteArray);
};
class PrivateMessageDialog : public QDialog
{
	Q_OBJECT

	public:
		PrivateMessageDialog(); //TODO: add destructor
		QVBoxLayout *layout;
		CustomTextEdit* privatetext;
		QPushButton* sendbutton;

		public slots:
		void handleSendButton(){
			emit sendpm(privatetext->toPlainText());
		}
		signals:
		void sendpm(QString);
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();
		bool bind();																//bind to default port
		int myPortMin, myPortMax, myPort;
		QString readNewDatagram();
};
class Peer 																			//Holds information for a peer
{
	public:
		Peer(QHostAddress s,quint16 p,QString h="")
		{
			 sender = s; senderport = p; host = h;
		}
		QHostAddress sender;
		quint16 senderport;
		QString host;
};
//Main class for peerster instance
class ChatDialog : public QDialog
{
	Q_OBJECT

	public:
		ChatDialog();
		~ChatDialog();
		void addPeers(QStringList);		//Add peers entered as formatter strings
		void setnoforwarding(bool b){
			noforwarding = b;
		}
	public slots:
		void gotReturnPressed();			//Handles delivery of rumors orignating here
		void gotNewMessage();					//Handles readyRead() for incoming datagrams
		void handleSelectionChanged(int);//Handles activated() for origin list
		void handleShareFileButton();	//Handles released() for file sharing button
		void handleSendPm(QString);		//Send private message
		void handleDownloadButton();	//Generate dialog for requesting a download
		void handleDownloadFile(QString,QByteArray);//Send Download request
		void sendAntiEntropyStatus();	//Implementation of anti-entropy
		void sendRouteRumor();				//Implementation of periodic route rumoring
		void lookedUp(QHostInfo);			//Handles completed DNS lookups
		void addPeer();								//Add a new peer from hostline
		void handleSearchLine();
		void checkReceipt();					//Implements timeouts against
																	//non-responsive peers
		void handleItemDoubleClicked(QListWidgetItem*);
		void handleRebroadcastTimer();
	private:
		QGridLayout *gridlayout;
		QVBoxLayout *layout1;					//Layout of GUI
		QVBoxLayout * layout2;			
		QLineEdit* searchline;
		QTextEdit *textview; 					//Main display view for rumors sent and rcvd
		CustomTextEdit *textline;			//Text edit to enter new rumors
		QLineEdit *hostline;					//Line edit to enter new peers
		QComboBox *privatebox;				//List of origins for private messaging 
		QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
		QPushButton* downloadfilebutton;//Launch dialog for file download	
		QListWidget* listview;
		PrivateMessageDialog *privatedialog; //Dialog for sending private messages
		QFileDialog* filedialog;
		DownloadFileDialog* sharedialog;
		NetSocket sock;								//Netsocket instance to bind to a port
		quint32 seqno;								//Counter for rumors sent by this instance
		QString origin;								//Our random origin key
		QString destinationorigin;		//Destination for current private message
		QMap<QString,quint32> lastbudgetmap;
		QVariantMap currentrumor;			//Current rumor for rumor-mongering
		QList<Peer> *peerlist;				//list of known peers
		QMap<QString,quint16> unresolvedhostmap; //Hosts currently being lookedup
		QHash<QString,QPair<QHostAddress,quint16> > hophash;//next hop table(routing)
		QMap<QByteArray,QPair<QString,QString> > requestedmetafiles;
		QMap<QString,QPair<QString,QByteArray> > searchresultmap;
		QTimer *responsetimer;				//Timer for timeouts on unresponsive peer
		QTimer *entropytimer;					//Timer for firing off anti-entropy messages
		QTimer *routetimer;						//Timer for firing off periodic route rumors		
		QTimer *rebroadcasttimer;
		bool success;									//Verify if a response was rcvd before timeout			
		bool noforwarding;						//no-forwarding to test NAT traversal
		
		FileShareManager filemanager;
		void sendRumorMessage(QVariantMap);	//Implementation of rumormongering
		void sendStatusMessage(quint16 senderport, 				//Send status message to
			QHostAddress sender = QHostAddress::LocalHost);	//specified peer
		void sendBlockRequestMessage(QString,QByteArray); //Send a block request
		void writeToSocket(QVariantMap message, quint16 port,//Calls writeDatagram
		 QHostAddress host = QHostAddress::LocalHost);		//if forwading enabled
		QByteArray serializeToByteArray(QVariantMap);	//Serialze QVMap to QByteArray
		Peer chooseRandomPeer();	//Selects a random peer
		void addNewPeer(QHostAddress,quint16); //add a peer if new
		void sendRumorToAllPeers(QVariantMap); //Send Rumor to all Peers

		//Helper functions for gotNewMessage() to handle different types of rumors
		void handleStatusRumor(QVariantMap, QHostAddress, quint16);
		void handleChatRouteRumor(QVariantMap, QHostAddress, quint16);
		void handlePrivateBlockRumor(QVariantMap);
		//Handle reading of datagram, Deserialize, and return map,sender,senderport
		QVariantMap readDeserializeDatagram(QHostAddress*, quint16*);
		void sendBlockReplyMessage(QByteArray, QByteArray, QString);
		void handleSearch(QString,QString);
		void rebroadcastSearchRequest(QString,quint32 = 2);
		void sendSearchRequestMessage(QVariantMap);
		void sendSearchReplyMessage(QString,QString,QVariantList,QByteArray);
		void handleSearchReplyRumor(QVariantMap);
		void handleSearchRequestRumor(QVariantMap);

		//The recvdmessage map has origin key with multiple rumor messages as the
		//values. Each value is thus a QVMap: QVMap<"origin",QVMap(rumor message)>
		QVariantMap recvdmessagemap;
		//The status map has origin keys with a quint32 sequence to track the last
		//read message from origin. QVMap<"origin",seqno>
		QVariantMap statusmap;
	};

#endif // PEERSTER_MAIN_HH
