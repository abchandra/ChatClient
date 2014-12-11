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
//Main class for chordster instance
class ChordNode : public QDialog
{
	Q_OBJECT

public:
	ChordNode();

public slots:

private:
		static const quint32 idlength = CHORD_BITS;
		NetSocket sock;								//Netsocket instance to bind to a port
		
		quint32 nodeKey;							//chord key created from node ip and port
		QVBoxLayout *layout1;					//Layout of GUI
		QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
		QPushButton* downloadfilebutton;//Launch dialog for file download	

		void setCreateChordKey();			//Create and store the chord key for node
		quint32 getKeyFromName(QString);	//Create chord key from given QString
};

#endif // CHORDSTER_MAIN_HH
