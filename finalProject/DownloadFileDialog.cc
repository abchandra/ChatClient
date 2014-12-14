/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#include "DownloadFileDialog.hh"

DownloadFileDialog::DownloadFileDialog() {
	fileidtext = new CustomTextEdit(this);
	reqbutton = new QPushButton("Click to Request/Hit Return",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(fileidtext);
	layout->addWidget(reqbutton);
	fileidtext->setFocus(Qt::ActiveWindowFocusReason);
	connect(reqbutton,SIGNAL(released()),this, SLOT(handleRequestButton()));
	connect(fileidtext, SIGNAL(returnPressed()),this, SLOT(handleRequestButton()));
	setLayout(layout);
}

void DownloadFileDialog::handleRequestButton(){
	emit downloadfile(fileidtext->toPlainText().toUInt());
}
