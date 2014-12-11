#include <QUdpSocket>
#include <QHostInfo>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		QHostInfo *hostInfo;
		quint16 myPortMin, myPortMax, myPort;
		NetSocket();
		bool bind();																//bind to default port
		bool LookupOwnIP();

	public slots:
		void lookedUp(QHostInfo);			//Handles completed DNS lookups

	private:
		void delay();

};