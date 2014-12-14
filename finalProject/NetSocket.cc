/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#include "NetSocket.hh"
#include <unistd.h>
#include <QTime>
#include <QCoreApplication>
void NetSocket::delay(quint16 secs)
{
    QTime dieTime= QTime::currentTime().addSecs(secs);
    while( QTime::currentTime() < dieTime )
    	QCoreApplication::processEvents(QEventLoop::AllEvents, 100);    
}

NetSocket::NetSocket()
{
	//Pick a range of four UDP ports to try to allocate by default,
	//computed based on my Unix user ID.This makes it trivial for up 
	//to four Peerster instances per user to find each other on the 
	//same host, barring UDP port conflicts with other applications 
	//(which are quite possible).We use the range from 32768 to 49151.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
	hostInfo = NULL;

}

bool NetSocket::bind()
{
	//Try to bind to each of the range myPortMin to myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			myPort = p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}


bool NetSocket::LookupOwnIP(){

	QHostInfo::lookupHost(QHostInfo::localHostName(),this,SLOT(lookedUp(QHostInfo)));
	if (hostInfo==NULL)
		delay(2);
	if (!hostInfo || hostInfo->error() != QHostInfo::NoError) {
    qDebug() << "Lookup failed:" << hostInfo->errorString();
    return false;
     }
  return true;

}

void NetSocket::lookedUp(QHostInfo host)
{
	hostInfo=new QHostInfo(host);    
}
