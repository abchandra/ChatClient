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
	ChordNode dialog;									//Create an initial chat dialog window
	dialog.show();

	QStringList argumentlist = QCoreApplication::arguments();
																			//Enter the Qt main loop;	 
	return app.exec();									//everything else is event driven
}
