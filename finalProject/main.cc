/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#include <QApplication>

#include "ChordNode.hh"



int main(int argc, char **argv)
{
	QCA::Initializer qcainit;						//Initialize Crypto library
	QApplication app(argc,argv);				//Initialize Qt toolkit
	ChordNode* dialog;									//Create an initial chat dialog window

	QStringList argumentlist = QCoreApplication::arguments();
	if (argumentlist.size()>1){
		dialog=new ChordNode(true);
		dialog->handlehostEntered(argumentlist[1]);			
																			//Enter the Qt main loop;	 
	}
	else
		dialog=new ChordNode(false);
	dialog->show();

	return app.exec();									//everything else is event driven
}
