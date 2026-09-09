#pragma once
#include <QtWidgets/QTreeView>
#include <QModelIndex>
#include <QStandardItem>
#include <QStandardItemModel>
namespace MOON {
	class TreeViewPanel;
	
	class TreeViewPanel : public QTreeView
	{
		Q_OBJECT
	public:
	
		TreeViewPanel(QWidget* parent);
		~TreeViewPanel();

	public slots:
		// FilamentApp::sceneLoaded / sceneCleared
		void onSceneLoaded(const QString& filePath);
		void onSceneCleared();
		// FilamentApp::pickedEntityChanged（3D 视口悬浮拾取联动）
		void onPickedEntityChanged(int entityIndex);

	public:
		int pickedEntity() const { return mPickedEntity; }
		// 该行是否对应当前 3D 拾取命中的实体（画金黄色高亮）
		bool isPickedEntityRow(const QModelIndex& index) const;

	private slots:
		void onItemChanged(QStandardItem* item);
	
	protected:
		
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void leaveEvent(QEvent* event) override;

	private:
		class TreeViewPanelInternal;
		TreeViewPanelInternal* mInternal;
		QStandardItemModel* mModel = nullptr;
		QStandardItem* mRootItem = nullptr;
		bool mUpdating = false;   // 程序内部批量刷新时屏蔽 itemChanged 递归
		int mHoveredEntity = -1;  // 当前悬浮的实体全局下标，-1 表示无
		int mPickedEntity = -1;   // 3D 视口拾取命中的实体，-1 表示无
	};
}
