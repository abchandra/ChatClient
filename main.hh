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
#include <QFileDialog>

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
		void handleFileButton();			//Handles released() for file sharing button
		void handleSendPm(QString);		//Send private message
		void sendAntiEntropyStatus();	//Implementation of anti-entropy
		void sendRouteRumor();				//Implementation of periodic route rumoring
		void lookedUp(QHostInfo);			//Handles completed DNS lookups
		void addPeer();								//Add a new peer from hostline
		void checkReceipt();					//Implements timeouts against
																	//non-responsive peers

	private:
		QVBoxLayout *layout;					//Layout of GUI
		QTextEdit *textview; 					//Main display view for rumors sent and rcvd
		CustomTextEdit *textline;			//Text edit to enter new rumors
		QLineEdit *hostline;					//Line edit to enter new peers
		QComboBox *privatebox;				//List of origins for private messaging 
		QPushButton* filebutton;			//Button to launch dialog for file sharing
		PrivateMessageDialog *privatedialog; //Dialog for sending private messages
		QFileDialog* filedialog;
		NetSocket sock;								//Netsocket instance to bind to a port
		quint32 seqno;								//Counter for rumors sent by this instance
		QString origin;								//Our random origin key
		QString destinationorigin;		//Destination for current private message
		QVariantMap currentrumor;			//Current rumor for rumor-mongering
		QList<Peer> *peerlist;				//list of known peers
		QMap<QString,quint16> unresolvedhostmap; //Hosts currently being lookedup
		QHash<QString,QPair<QHostAddress,quint16> > hophash;//next hop table(routing)

		QTimer *responsetimer;				//Timer for timeouts on unresponsive peer
		QTimer *entropytimer;					//Timer for firing off anti-entropy messages
		QTimer *routetimer;						//Timer for firing off periodic route rumors		
		bool success;									//Verify if a response was rcvd before timeout			
		bool noforwarding;						//no-forwarding to test NAT traversal
		
		void sendRumorMessage(QVariantMap);	//Implementation of rumormongering
		void sendStatusMessage(quint16 senderport, 				//Send status message to
			QHostAddress sender = QHostAddress::LocalHost);	//specified peer
		void writeToSocket(QVariantMap message, quint16 port,//Calls writeDatagram
		 QHostAddress host = QHostAddress::LocalHost);		//if forwading enabled
		QByteArray serializeToByteArray(QVariantMap);	//Serialze QVMap to QByteArray
		Peer chooseRandomPeer();	//Selects a random peer
		void addNewPeer(QHostAddress,quint16); //add a peer if new
		void SendRumorToAllPeers(QVariantMap); //Send Rumor to all Peers

		//Helper functions for gotNewMessage() to handle different types of rumors
		void HandleStatusRumor(QVariantMap, QHostAddress, quint16);
		void HandleChatRouteRumor(QVariantMap, QHostAddress, quint16);
		void HandlePrivateMessageRumor(QVariantMap);
		//Handle reading of datagram, Deserialize, and return map,sender,senderport
		QVariantMap ReadDeserializeDatagram(QHostAddress*, quint16*);

		//The recvdmessage map has origin key with multiple rumor messages as the
		//values. Each value is thus a QVMap: QVMap<"origin",QVMap(rumor message)>
		QVariantMap recvdmessagemap;
		//The status map has origin keys with a quint32 sequence to track the last
		//read message from origin. QVMap<"origin",seqno>
		QVariantMap statusmap;
	};

#endif // PEERSTER_MAIN_HH
