#include "FileShareManager.hh"
#include <QVariant>
#include <QMessageBox>
#include <QDebug>
QList<QByteArray> FileShareManager::addFiles(QStringList filenames) {
	QList<QByteArray> addedfilesHashList;
	QStringList::iterator i;
	for (i=filenames.begin(); i!= filenames.end();++i){
		FileData f=prepareFileData(*i);
		if(!f.blocklisthash.isEmpty())
			addedfilesHashList.append(f.blocklisthash);
	}
	return addedfilesHashList;
}
FileData FileShareManager::prepareFileData(QString filename) {
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)){
		FileData f; f.blocklisthash=QByteArray();
		return f;
	}
		FileData filedata;
		QStringList splitlist = filename.split("/");
		filedata.filename = splitlist.last();
		QByteArray blocks = file.readAll();
		file.close();
		filedata.filesize = blocks.size();
		QByteArray blockiterator = blocks;
		QByteArray blocklistfile;
		while(blockiterator.size()){
			QByteArray block = blockiterator.left(const_blocksize); //get first block
			if (blockiterator.size() <=const_blocksize)
				blockiterator = ""; //Now empty
			else
				blockiterator = blockiterator.mid(const_blocksize); //Shrink blockiterator
			QByteArray blockhash = QCA::Hash("sha1").hash(block).toByteArray(); //hash the block
			blocklistfile.append(blockhash);
			filedata.blocklookuptable.insert(blockhash,block);
		}
		filedata.blocklistfile = blocklistfile;
		filedata.blocklisthash = QCA::Hash("sha1").hash(blocklistfile).toByteArray();
		sharedfiles.append(filedata);
		return filedata;
		qDebug()<<"uploaded:"<<filedata.blocklisthash.toHex();
}

QByteArray* FileShareManager::findBlockFromHash(QByteArray hashval) {
	QList<FileData>::iterator i;
	for (i=sharedfiles.begin(); i!= sharedfiles.end();++i){
		QByteArray* result = i->lookupHash(hashval);
		if (result)
			return result;
	}
	return NULL;
}

QByteArray* FileData::lookupHash(QByteArray hashval) {
	if (blocklisthash == hashval){
		QByteArray* result = new QByteArray(blocklistfile);
		return result;
	}
	if (blocklookuptable.contains(hashval)) {
		QByteArray* result = new QByteArray(blocklookuptable.value(hashval));
		return result;
	}
	return NULL;
}

bool FileShareManager::sanityCheck(QByteArray hashval, QByteArray block){
	return (QCA::Hash("sha1").hash(block).toByteArray() == hashval);
}

QByteArray FileShareManager::addDownload(QByteArray blocklisthash,QByteArray blocklistfile, 
	QString host,QString filename) {
	FileData filedata;
	filedata.filename=filename;
	filedata.blocklisthash = blocklisthash;
	filedata.blocklistfile = blocklistfile;
	filedata.nextdownloadblock=blocklistfile;
	filedata.hostorigin = host;
	if (blocklistfile.isEmpty()) {
		writeToFile(filedata);
		return "";
	}
	inprogressdownloads.append(filedata);
	return nexthashval(filedata.nextdownloadblock);

}
QByteArray FileShareManager::addBlock(QByteArray hashval, QByteArray block, QString source) {
		QList<FileData>::iterator i;
	for (i=inprogressdownloads.begin(); i!= inprogressdownloads.end();++i){
		if (i->nextdownloadblock.left(const_hashsize)==hashval && source==i->hostorigin){
			i->blocks.append(block);
			i->blocklookuptable.insert(hashval,block);
			if (i->nextdownloadblock.size() <=const_hashsize){
				writeToFile(*i);
				inprogressdownloads.erase(i);
				return "";
			}
			else{
			i->nextdownloadblock=i->nextdownloadblock.mid(const_hashsize); //next block
			return nexthashval(i->nextdownloadblock);
			}
		}
	}
	return "";
}

QByteArray FileShareManager::nexthashval(QByteArray hasharray){
	if (hasharray.isEmpty())
		return "";
	QByteArray next = hasharray.left(const_hashsize);
	return next;
}

void FileShareManager::writeToFile(FileData filedata){
	QString fileid;
	if (filedata.filename.isEmpty())
	 fileid = filedata.blocklisthash.toHex();
	else
	 fileid = filedata.filename;
	QFile file(fileid);
	file.open(QIODevice::WriteOnly);
	file.write(filedata.blocks);
	file.close();
	QMessageBox msgBox;
	msgBox.setText("Successfully Downloaded "+fileid);
	msgBox.exec();
}

void FileShareManager::keywordSearch(QString querystring,QVariantList& matches,QVariantList& result){
	QString query = querystring.toLower();
	QList<FileData>::iterator i;
	for (i=sharedfiles.begin(); i!= sharedfiles.end();++i){
		QStringList splitlist = i->filename.split(" ");
		QStringList::iterator j;
		for (j=splitlist.begin();j!=splitlist.end();++j){
			if ((j->toLower()).contains(query)){
				matches.append(QVariant(i->filename));
				result.append(QVariant(i->blocklisthash));
				break;
			}
		}
	}
}
