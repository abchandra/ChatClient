#include "CustomTextEdit.hh"

void CustomTextEdit::keyPressEvent(QKeyEvent* e) {
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
		emit returnPressed();
	else 
		QTextEdit::keyPressEvent(e);
} 
