#include "FileShareManager.hh"
#include <QDebug>

void FileShareManager::addFiles(QStringList filenames) {
	QStringList::iterator i;
	for (i=filenames.begin(); i!= filenames.end();++i)
		prepareFileData(*i);
}
void FileShareManager::prepareFileData(QString filename) {
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
		return;

		FileData filedata;
		filedata.filename = filename;
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
		//qDebug()<<"file:"<<filedata.filename<<"##size:"<<filedata.blocklistfile.size()/20<<"##blfhash:"<<filedata.blocklisthash.toHex();
}

QByteArray* FileShareManager::findBlockFromHash(QByteArray hashval) {
		//qDebug()<<"hashval:"<<hashval.toHex();
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

QByteArray FileShareManager::addDownload(QByteArray blocklisthash,QByteArray blocklistfile, QString host) {
	FileData filedata;
	filedata.blocklisthash = blocklisthash;
	filedata.blocklistfile = blocklistfile;
	filedata.nextdownloadblock=blocklistfile;
	filedata.hostorigin = host;
	inprogressdownloads.append(filedata);
	return nexthashval(filedata.nextdownloadblock);

}
QByteArray FileShareManager::addBlock(QByteArray hashval, QByteArray block, QString source) {
		QList<FileData>::iterator i;
	for (i=inprogressdownloads.begin(); i!= inprogressdownloads.end();++i){
		if (i->nextdownloadblock.left(const_hashsize)==hashval && source==i->hostorigin){
			//qDebug()<<"newblock"<<hashval.toHex()<< "from "<<source;
			i->blocks.append(block);
			i->blocklookuptable.insert(hashval,block);
			if (i->nextdownloadblock.size() <=const_hashsize){
				writeToFile(*i);
				inprogressdownloads.erase(i);
				//qDebug()<<"WOOT DONE!";
				return "";//TODO: Write file and remove from this list
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
	//qDebug()<<"NEXT GEN"<<next.toHex();
	return next;
}

void FileShareManager::writeToFile(FileData filedata){
	QString fileid = filedata.blocklisthash.toHex();
	QFile file(fileid);
	file.open(QIODevice::WriteOnly);
	file.write(filedata.blocks);
	file.close();
}
