#ifndef PEERSTER_FILESHAREMANAGER_HH
#define PEERSTER_FILESHAREMANAGER_HH

#include <QFile>
#include <QtCrypto>
#define BLOCKSIZE 8192
#define HASHSIZE 20

class FileData {
public:
	QString filename;
	QString hostorigin;
	quint32 filesize;
	QHash<QByteArray,QByteArray> blocklookuptable;
	QByteArray blocks;
	QByteArray blocklistfile;
	QByteArray nextdownloadblock;
	QByteArray blocklisthash;
	QByteArray* lookupHash(QByteArray);
private:
	static const int const_blocksize = BLOCKSIZE;

};

class FileShareManager {
public:
	void addFiles(QStringList);
	QByteArray* findBlockFromHash(QByteArray);
	bool sanityCheck(QByteArray,QByteArray);
	QByteArray addDownload(QByteArray,QByteArray,QString);
	QByteArray addBlock(QByteArray,QByteArray,QString);
	QByteArray nexthashval(QByteArray);
	void writeToFile(FileData);


private:
	static const int const_blocksize = BLOCKSIZE;
	static const int const_hashsize = HASHSIZE;
	QList<FileData> sharedfiles;
	QList<FileData> inprogressdownloads;
	void prepareFileData(QString);

};

#endif