/* 
Abhishek Chandra
Chordster
15/12/2014
*/
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
	DownloadFileDialog();

private:
	QVBoxLayout *layout;
	CustomTextEdit *fileidtext;
	QPushButton* reqbutton;


public slots:
	void handleRequestButton();
signals:
	void downloadfile(quint32);
};

