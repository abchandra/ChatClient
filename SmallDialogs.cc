#include "SmallDialogs.hh"

DownloadFileDialog::DownloadFileDialog(QStringList origins) {
	//set the default destination as empty
	destinationorigin="";
	//Populate list of destinations
	origins.insert(0,"Select Origin to Request...");
	fileidtext = new CustomTextEdit(this);
	destinationbox=new QComboBox(this);
	destinationbox->addItems(origins);
	reqbutton = new QPushButton("Click to Request/Hit Return",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(fileidtext);
	layout->addWidget(destinationbox);
	layout->addWidget(reqbutton);
	fileidtext->setFocus(Qt::ActiveWindowFocusReason);
	connect(destinationbox,SIGNAL(activated(int)),this,SLOT(handleSelectionChanged(int)));
	connect(reqbutton,SIGNAL(released()),this, SLOT(handleRequestButton()));
	connect(fileidtext, SIGNAL(returnPressed()),this, SLOT(handleRequestButton()));
	setLayout(layout);
}

void DownloadFileDialog::handleRequestButton(){
	emit downloadfile(destinationorigin, fileidtext->toPlainText().toAscii());
}

void DownloadFileDialog::handleSelectionChanged(int index){
	if (index==0)
		destinationorigin = "";
	else
		destinationorigin = destinationbox->itemText(index);
}


PrivateMessageDialog::PrivateMessageDialog() {
	setWindowTitle("New Private Message");
	privatetext = new CustomTextEdit(this);
	sendbutton = new QPushButton("Click to Send/Hit Return",this);
	layout = new QVBoxLayout(this);
	layout->addWidget(privatetext);
	layout->addWidget(sendbutton);
	privatetext->setFocus(Qt::ActiveWindowFocusReason);
	connect(sendbutton,SIGNAL(released()),this,SLOT(handleSendButton()));
	connect(privatetext, SIGNAL(returnPressed()),this, SLOT(handleSendButton()));
	setLayout(layout);
}