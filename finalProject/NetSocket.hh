
#include <QUdpSocket>


class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();
		bool bind();																//bind to default port
		int myPortMin, myPortMax, myPort;
		QString readNewDatagram();
};