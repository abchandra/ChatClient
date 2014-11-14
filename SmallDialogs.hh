
#include "CustomTextEdit.hh"

#include <QDialog>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QString>

class DownloadFileDialog : public QDialog
{
	Q_OBJECT
public:
	DownloadFileDialog(QStringList);

private:
	QVBoxLayout *layout;
	QComboBox *destinationbox;
	CustomTextEdit *fileidtext;
	QPushButton* reqbutton;
	QString destinationorigin;

public slots:
	void handleRequestButton();
	void handleSelectionChanged(int);
signals:
	void downloadfile(QString,QByteArray);
};


class PrivateMessageDialog : public QDialog
{
	Q_OBJECT

	public:
		PrivateMessageDialog(); //TODO: add destructor
		QVBoxLayout *layout;
		CustomTextEdit* privatetext;
		QPushButton* sendbutton;

		public slots:
		void handleSendButton(){
			emit sendpm(privatetext->toPlainText());
		}
		signals:
		void sendpm(QString);
};