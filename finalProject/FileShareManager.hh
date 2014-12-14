/* 
Abhishek Chandra
Chordster
15/12/2014
*/
#ifndef PEERSTER_FILESHAREMANAGER_HH
#define PEERSTER_FILESHAREMANAGER_HH

#include <QFile>
#include <QtCrypto>
#define BLOCKSIZE 8192
#define HASHSIZE 20

class FileData {
public:
	QString filename;
	quint32 hostKey;
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
	QList<QByteArray> addFiles(QStringList);
	QByteArray* findBlockFromHash(QByteArray);
	bool sanityCheck(QByteArray,QByteArray);
	QByteArray addDownload(QByteArray,QByteArray,quint32,QString);
	QByteArray addBlock(QByteArray,QByteArray,quint32);
	QByteArray nexthashval(QByteArray);
	void writeToFile(FileData);
	void keywordSearch(QString,QVariantList&,QVariantList&);

private:
	static const int const_blocksize = BLOCKSIZE;
	static const int const_hashsize = HASHSIZE;
	QList<FileData> sharedfiles;
	QList<FileData> inprogressdownloads;
	FileData prepareFileData(QString);

};

#endif