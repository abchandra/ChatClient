/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#include "DownloadFileDialog.hh"

DownloadFileDialog::DownloadFileDialog() {
	fileidtext = new CustomTextEdit(this);
	reqbutton = new QPushButton("Click to Request",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(fileidtext);
	layout->addWidget(reqbutton);
	fileidtext->setFocus(Qt::ActiveWindowFocusReason);
	connect(reqbutton,SIGNAL(released()),this, SLOT(handleRequestButton()));
	connect(fileidtext, SIGNAL(returnPressed()),this, SLOT(handleRequestButton()));
	setLayout(layout);
}

ConnectDialog::ConnectDialog(){
	hostline=new QLineEdit(this);
	hostline->setPlaceholderText("Enter new Host here");
	reqbutton = new QPushButton("Click to Connect",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(hostline);
	layout->addWidget(reqbutton);
	connect(reqbutton,SIGNAL(released()),this, SLOT(handleRequestButton()));
	connect(hostline, SIGNAL(returnPressed()),this, SLOT(handleRequestButton()));
	setLayout(layout);
}



void DownloadFileDialog::handleRequestButton(){
	emit downloadfile(fileidtext->toPlainText().toUInt());
}
void ConnectDialog::handleRequestButton(){
	emit hostEntered(hostline->text());
}