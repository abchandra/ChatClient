#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QUdpSocket>


//Text Edit for chat line
//TODO: 2-3 line bug. Consider QtPlainTextEdit
class CustomTextEdit : public QTextEdit
{
	Q_OBJECT
	public:
		CustomTextEdit(QWidget* parent = 0): 
			QTextEdit(parent = 0){}; 
	protected:
		void keyPressEvent(QKeyEvent *event);
	signals:
		void returnPressed();
};


class ChatDialog : public QDialog
{
	Q_OBJECT

	public:
		ChatDialog();

	public slots:
		void gotReturnPressed();

	private:
		QTextEdit *textview;
		CustomTextEdit *textline;
	};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();

		// Bind this socket to a Peerster-specific default port.
		bool bind();

	private:
		int myPortMin, myPortMax;
};



#endif // PEERSTER_MAIN_HH
