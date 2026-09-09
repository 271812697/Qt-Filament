#include "openFile.h"

#include <fstream>
#include <QtWidgets/QFileDialog>
#include <QCoreApplication>
namespace MOON {


	//-----------------------------------------------------------------------------
	OpenFileCommand::OpenFileCommand(QObject* parentObject)
		: Command(parentObject)
	{
		
		
		
		//connect(this, &OpenFileCommand::readFilePath, &viewer, &ViewerWidget::onReadFile);
		auto openfile=new QAction(this);
		setAction(openfile);
		openfile->setObjectName(QString::fromUtf8("actionFileOpen"));
		openfile->setText("&Open");
		openfile->setStatusTip("Open");
		openfile->setShortcut(QCoreApplication::translate("pqFileMenuBuilder", "Ctrl+O", nullptr));
		QIcon icon9;
		icon9.addFile(QString::fromUtf8(":/widgets/icons/pqOpen.svg"), QSize(), QIcon::Normal, QIcon::Off);
		openfile->setIcon(icon9);
	}

	void OpenFileCommand::execute()
	{
		QString fileName = QFileDialog::getOpenFileName(nullptr,
			tr("Open Flow Scene"),
			QDir::homePath(),
			tr("Flow Scene Files (*.bin;*.*)"));
		if (!QFileInfo::exists(fileName))
			return;
		
		emit readFilePath(fileName);
	}
}




