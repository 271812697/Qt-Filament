#pragma once
#include "treeViewpanel.h"
#include "core/FilamentApp.h"
#include "editor/UI/TreeViewPanel/EntityTreeStyle.h"
#include "editor/UI/PropertyPanel/PropertyWidget.h"

#include <QFileInfo>
#include <QFileSystemModel>
#include <QAbstractItemModel>
#include <QColor>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <string>
#include <vector>

namespace MOON {
	static bool isEntityCheckAble(const std::string& name) {
		if (name == "HeadLight" || name == "PointLight1" || name == "PointLight2" || name == "PointLight3" || name == "PointLight4") {
			return false;
		}
		return true;
	}
	class HighlightDelegate : public QStyledItemDelegate
	{
	public:
		explicit HighlightDelegate(TreeViewPanel* panel, QObject* parent = nullptr)
			: QStyledItemDelegate(parent), m_panel(panel) {
		}

		void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			QStyleOptionViewItem opt = option;
			if (m_panel && m_panel->isPickedEntityRow(index)) {
				// 3D 视口拾取命中的行：金黄色底 + 深色文字
				painter->save();
				painter->fillRect(option.rect, QColor("#ffd700"));
				painter->restore();
				opt.backgroundBrush = QColor("#ffd700");
				opt.palette.setColor(QPalette::Text, QColor("#1f1f1f"));
				opt.palette.setColor(QPalette::HighlightedText, QColor("#1f1f1f"));
			}
			QStyledItemDelegate::paint(painter, opt, index);
		}

	private:
		TreeViewPanel* m_panel;
	};
	class TreeViewPanel::TreeViewPanelInternal {
	public:
		TreeViewPanelInternal(TreeViewPanel* tree) :mSelf(tree) {
			
		}
		~TreeViewPanelInternal() {
		}
	private:
		friend TreeViewPanel;
		TreeViewPanel* mSelf = nullptr;
		QModelIndex m_lastIndex;  // 记录上一次悬浮项
	};
	TreeViewPanel::TreeViewPanel(QWidget* parent) :QTreeView(parent), mInternal(new TreeViewPanelInternal(this))
	{
		
		QSizePolicy sizePolicy8(QSizePolicy::Preferred, QSizePolicy::Expanding);
		sizePolicy8.setHorizontalStretch(0);
		sizePolicy8.setVerticalStretch(0);
		sizePolicy8.setWidthForHeight(true);
		sizePolicy8.setHeightForWidth(this->sizePolicy().hasHeightForWidth());
		this->setSizePolicy(sizePolicy8);
		mModel = new QStandardItemModel(this);
		this->setModel(mModel);
		this->setItemDelegate(new EntityTreeViewStyleDelegate(this));
		this->header()->hide();
		this->setStyleSheet(R"(
    QTreeView::indicator:checked {
        image: url(:/entityTree/icons/pqEyeball.svg);
    }
    QTreeView::indicator:unchecked {
        image: url(:/entityTree/icons/pqEyeballClosed.svg);
    }
    QTreeView::item {
        height: 20px;
        padding-left: 4px;
    }
    QTreeView::item:hover {
        background-color: #ffd700;  /* 悬浮金黄色高亮 */
        color: #1f1f1f;
    }
    QTreeView::item:selected {
        background-color: #7ab2e8;   /* 选中颜色 */
        color: white;
    }
    QTreeView::branch {
        background: transparent;
    }
    QTreeView::branch:has-siblings:!adjoins-item,
    QTreeView::branch:has-siblings:adjoins-item,
    QTreeView::branch:!has-children:!has-siblings:adjoins-item {
        border-image: none;
    }
    QTreeView::branch:has-children:!has-siblings:closed,
    QTreeView::branch:closed:has-children:has-siblings {
        border-image: none;
        image: url(:/widgets/icons/arrow_right.svg);
    }
    QTreeView::branch:open:has-children:!has-siblings,
    QTreeView::branch:open:has-children:has-siblings {
        border-image: none;
        image: url(:/widgets/icons/arrow_down.svg);
    }
)");
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);   // 获得焦点
		viewport()->setAttribute(Qt::WA_Hover); // 关键：让视图识别hov
		setItemDelegate(new HighlightDelegate(this, this));
		setSelectionBehavior(QAbstractItemView::SelectRows);
		setSelectionMode(QAbstractItemView::SingleSelection);

		// 👇 这一句是关键！禁止点行触发勾选
		setEditTriggers(QAbstractItemView::NoEditTriggers);

		// 加载文件后重建实体树；小眼睛勾选控制实体可见性
		connect(mModel, &QStandardItemModel::itemChanged,
			this, &TreeViewPanel::onItemChanged);
		connect(&FilamentApp::instance(), &FilamentApp::sceneLoaded,
			this, &TreeViewPanel::onSceneLoaded);
		connect(&FilamentApp::instance(), &FilamentApp::sceneCleared,
			this, &TreeViewPanel::onSceneCleared);
		connect(&FilamentApp::instance(), &FilamentApp::pickedEntityChanged,
			this, &TreeViewPanel::onPickedEntityChanged);

		// 选中行 → 通知 FilamentApp（PropertyWidget 据此显示实体组件）
		connect(selectionModel(), &QItemSelectionModel::currentChanged,
			this, [this](const QModelIndex& current, const QModelIndex&) {
				int entityIndex = -1;
				if (current.isValid()) {
					if (QStandardItem* item = mModel->itemFromIndex(current)) {
						if (item->parent()) {
							entityIndex = item->data(Qt::UserRole).toInt();
						}
					}
				}
				FilamentApp::instance().setSelectedEntity(entityIndex);
			});
	}
	TreeViewPanel::~TreeViewPanel()
	{
		delete mInternal;
	}

	void TreeViewPanel::onSceneLoaded(const QString& filePath) {
		mUpdating = true;

		QString rootName = QFileInfo(filePath).completeBaseName();
		if (rootName.isEmpty()) {
			rootName = "Scene";
		}

		FilamentApp& app = FilamentApp::instance();
		const size_t start = app.lastLoadedEntityStart();
		const size_t end = app.entityCount();

		auto* root = new QStandardItem(rootName);
		root->setCheckable(true);
		root->setCheckState(Qt::Checked);
		root->setEditable(false);
		root->setData(int(start), Qt::UserRole);        // 本文件实体起始下标
		root->setData(int(end - start), Qt::UserRole + 1); // 本文件实体数量
		mRootItem = root;
		mModel->appendRow(root);

		for (size_t i = start; i < end; i++) {
			auto* item = new QStandardItem(QString("Mesh %1").arg((qulonglong)i));
			item->setCheckable(true);
			item->setCheckState(app.isEntityVisible(i) ? Qt::Checked : Qt::Unchecked);
			item->setEditable(false);
			item->setData(int(i), Qt::UserRole);
			root->appendRow(item);
		}

		mUpdating = false;
		expandAll();
	}

	void TreeViewPanel::onSceneCleared() {
		mUpdating = true;
		mModel->clear();
		mRootItem = nullptr;
		mUpdating = false;
		mHoveredEntity = -1;
		mPickedEntity = -1;
	}

	void TreeViewPanel::onPickedEntityChanged(int entityIndex) {
		if (mPickedEntity != entityIndex) {
			mPickedEntity = entityIndex;
			viewport()->update();
		}
	}

	bool TreeViewPanel::isPickedEntityRow(const QModelIndex& index) const {
		if (mPickedEntity < 0 || !index.isValid() || !mModel) return false;
		QStandardItem* item = mModel->itemFromIndex(index);
		return item && item->parent() &&
			item->data(Qt::UserRole).toInt() == mPickedEntity;
	}

	void TreeViewPanel::onItemChanged(QStandardItem* item) {
		if (mUpdating || !item) return;

		const bool checked = item->checkState() == Qt::Checked;
		FilamentApp& app = FilamentApp::instance();

		if (!item->parent()) {
			// 根节点眼睛：控制本文件追加的那批实体
			const int start = item->data(Qt::UserRole).toInt();
			const int count = item->data(Qt::UserRole + 1).toInt();
			for (int k = 0; k < count; k++) {
				app.setEntityVisible((size_t)(start + k), checked);
			}
			mUpdating = true;
			for (int row = 0; row < item->rowCount(); row++) {
				if (auto* child = item->child(row)) {
					child->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
				}
			}
			mUpdating = false;
		} else {
			// 子节点眼睛：控制单个实体
			app.setEntityVisible((size_t)item->data(Qt::UserRole).toInt(), checked);
		}
	}

	
	void TreeViewPanel::mousePressEvent(QMouseEvent* event)
	{
		QTreeView::mousePressEvent(event);   // 让复选框/眼睛可点击
	}
	void TreeViewPanel::mouseMoveEvent(QMouseEvent* event)
	{
		QTreeView::mouseMoveEvent(event);

		// 悬浮实体行时，把 Filament 场景里对应的实体临时高亮为金黄色
		const QModelIndex index = indexAt(event->pos());
		int entityId = -1;
		if (index.isValid()) {
			if (QStandardItem* item = mModel->itemFromIndex(index)) {
				if (item->parent()) {   // 只有实体子节点对应单个场景实体
					entityId = item->data(Qt::UserRole).toInt();
				}
			}
		}
		if (entityId != mHoveredEntity) {
			if (mHoveredEntity >= 0) {
				FilamentApp::instance().setEntityHighlighted(
					(size_t)mHoveredEntity, false);
			}
			if (entityId >= 0) {
				FilamentApp::instance().setEntityHighlighted(
					(size_t)entityId, true);
			}
			mHoveredEntity = entityId;
		}
	}
	void TreeViewPanel::leaveEvent(QEvent* event)
	{
		if (mHoveredEntity >= 0) {
			FilamentApp::instance().setEntityHighlighted(
				(size_t)mHoveredEntity, false);
			mHoveredEntity = -1;
		}
		QTreeView::leaveEvent(event);
	}
}
