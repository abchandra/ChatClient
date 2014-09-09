#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QUdpSocket>
#include <QMap>
#include <QDataStream>
#include <QHostInfo>

typedef QString origindt;
typedef quint32 seqnodt;
//Text Edit for chat line
//TODO: 2-3 line bug. Consider QtPlainTextEdit
class CustomTextEdit : public QTextEdit
{
	Q_OBJECT
	public:
		CustomTextEdit(QWidget* parent = 0):
			QTextEdit(parent = 0){} 
	protected:
		void keyPressEvent(QKeyEvent* event);
	signals:
		void returnPressed();
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();

		// Bind this socket to a Peerster-specific default port.
		bool bind();
		int getmyPortMin(){return myPortMin;}
		int getmyPortMax(){return myPortMax;}
		int myPortMin, myPortMax, myPort;	//TODO: Privatize
		QString readNewDatagram();
		//void incrementSeqno();
		//quint32 getSeqno(){return seqno;}
	private:
		// public slots:


};

class ChatDialog : public QDialog
{
	Q_OBJECT

	public:
		ChatDialog();


	public slots:
		void gotReturnPressed();
		void gotNewMessage();
		

	private:
		QTextEdit *textview; 
		CustomTextEdit *textline;
		NetSocket sock;
		seqnodt seqno;
		origindt origin;
		QVariantMap currentrumor;
		quint32 generateRandom();
		void sendRumorMessage(QVariantMap msg);
		void sendStatusMessage(quint16 senderPort, QHostAddress sender);
		void writeToSocket(QByteArray data, quint16 port,
		 QHostAddress host = QHostAddress::LocalHost);
		QByteArray serializeToByteArray(QVariantMap);
		//The recvdmessage map has origin key
		//with multiple rumor messages as the
		//values. Each value is thus a QVMap.
		//QVMap<"origin",QVMap(rumor message)>
		QVariantMap recvdmessagemap;
		//The status map has origin keys with
		//a quint32 sequence to track the last
		//read message from origin.
		//QVMap<"origin",seqno>
		QVariantMap statusmap;
	};

// class MessageMap
// {
// 	public :
// 		MessageMap();
// 	private :
// 		QVariantMap message;
// 	public	:
// }



#endif // PEERSTER_MAIN_HH


/*
NOTES:
writeToSocket line 174 needs a destination
*/