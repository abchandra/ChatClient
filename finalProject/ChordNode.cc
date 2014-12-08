#include "ChordNode.hh"
#include "NetSocket.hh"
#include <unistd.h>
#include <QDebug>
#include <QApplication>
#include <QDateTime>

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

}