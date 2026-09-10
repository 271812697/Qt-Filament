#include "editor/Command/menubar/exportFile.h"
#include <QtWidgets/QFileDialog>
#include <QCoreApplication>
namespace MOON {
	//-----------------------------------------------------------------------------
	ExportFileCommand::ExportFileCommand(QObject* parentObject)
		: Command(parentObject)
	{
		auto openfile=new QAction(this);
		setAction(openfile);
		openfile->setObjectName(QString::fromUtf8("actionFileOpen"));
		openfile->setText("&Export");
		openfile->setStatusTip("Export");
		openfile->setShortcut(QCoreApplication::translate("pqFileMenuBuilder", "Ctrl+S", nullptr));
		QIcon icon9;
		icon9.addFile(QString::fromUtf8(":/widgets/icons/pqOpen.svg"), QSize(), QIcon::Normal, QIcon::Off);
		openfile->setIcon(icon9);
	}

	void ExportFileCommand::execute()
	{
		QString selectedFilter;
		QString fileName = QFileDialog::getSaveFileName(nullptr,
			tr("Export File"),
			QDir::homePath(),
			tr("STL File (*.stl);;STEP File (*.step)"), // 格式分开写
			&selectedFilter);
		if (fileName.isEmpty())
			return;
		

		

		// ======================
		// 关键：自动识别后缀
		// ======================
		QString ext = QFileInfo(fileName).suffix().toLower();
		if (ext.isEmpty()) {
			// 用户没写后缀 → 根据选择的过滤器自动补全
			if (selectedFilter.contains("*.stl"))
				fileName += ".stl";
			else if (selectedFilter.contains("*.step"))
				fileName += ".step";
		}

		// 重新获取后缀
		ext = QFileInfo(fileName).suffix().toLower();
		// ======================

	}
}




