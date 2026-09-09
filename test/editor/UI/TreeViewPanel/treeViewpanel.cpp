#pragma once
#include "treeViewpanel.h"
#include "editor/UI/TreeViewPanel/EntityTreeStyle.h"
#include "editor/UI/PropertyPanel/PropertyWidget.h"

#include <QFileSystemModel>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QMouseEvent>
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
		//this->setModel(mInternal->mModel);
		//this->setItemDelegate(new EntityTreeViewStyleDelegate(this));
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
        background-color: #cfe2f5;   /* 悬浮浅蓝，可自己改颜色 */
        color: #202020;
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
	}
	TreeViewPanel::~TreeViewPanel()
	{
		delete mInternal;
	}

	
	void TreeViewPanel::mousePressEvent(QMouseEvent* event)
	{
	
	}
	void TreeViewPanel::mouseMoveEvent(QMouseEvent* event)
	{
		QTreeView::mouseMoveEvent(event);
	}
}
