/***********************************
Abhishek Chandra
CPSC 426 lab1 
09/12/2014

A basic Peerster implementation
************************************/
#include <QApplication>
#include "ChatDialog.hh"


int main(int argc, char **argv)
{
	QCA::Initializer qcainit;						//Initialize Crypto library
	QApplication app(argc,argv);				//Initialize Qt toolkit
	ChatDialog dialog;									//Create an initial chat dialog window
	dialog.show();

	QStringList argumentlist = QCoreApplication::arguments();
	argumentlist.removeAt(0);
		dialog.setnoforwarding(false);
	dialog.addPeers(argumentlist);			//Add Peers entered on command line
																			
																			//Enter the Qt main loop;	 
	return app.exec();									//everything else is event driven
}
