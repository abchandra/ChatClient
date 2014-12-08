/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/

#ifndef CHORDSTER_MAIN_HH
#define CHORDSTER_MAIN_HH

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

//Main class for peerster instance
class ChordNode : public QDialog
{
	Q_OBJECT

public:
	ChordNode();

private:
		QVBoxLayout *layout1;					//Layout of GUI
		QPushButton* sharefilebutton;	//Button to launch dialog for file sharing
		QPushButton* downloadfilebutton;//Launch dialog for file download	
};

#endif // CHORDSTER_MAIN_HH
