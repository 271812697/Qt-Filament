#pragma once
#include <QPlainTextEdit>
#include <QRect>

class QPaintEvent;
class QShowEvent;
class QWidget;

namespace MOON {

	// 只读 GLSL 查看器：等宽字体、行号、无换行（长行横向滚动）。
	// 只负责显示，不提供编辑/保存入口。
	class ShaderCodeEditor : public QPlainTextEdit
	{
		Q_OBJECT
	public:
		explicit ShaderCodeEditor(QWidget* parent = nullptr);

		void SetSource(const QString& source);
		void SetEditable(bool editable);
		int LineNumberAreaWidth() const;
		void PaintLineNumbers(QPaintEvent* event);

	protected:
		void resizeEvent(QResizeEvent* event) override;
		void showEvent(QShowEvent* event) override;

	private slots:
		void UpdateLineNumberAreaWidth(int newBlockCount);
		void UpdateLineNumberArea(const QRect& rect, int dy);

	private:
		QWidget* mLineNumberArea = nullptr;
		bool mResetScrollOnShow = true;
	};

}
