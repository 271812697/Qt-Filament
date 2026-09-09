#pragma once
#include <QtWidgets/QTreeView>
#include <QModelIndex>
#include <QStandardItem>
namespace Core::ECS {
	class Actor;
}
namespace MOON {
	class TreeViewPanel;
	
	class TreeViewPanel : public QTreeView
	{
		Q_OBJECT
	public:
	
		TreeViewPanel(QWidget* parent);
		~TreeViewPanel();
	
	protected:
		
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;

	private:
		class TreeViewPanelInternal;
		TreeViewPanelInternal* mInternal;
	};
}