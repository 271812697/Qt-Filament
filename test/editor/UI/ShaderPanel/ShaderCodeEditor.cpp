#include "ShaderCodeEditor.h"

#include <QFont>
#include <QFrame>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>

#include <algorithm>

namespace MOON {
	namespace {

		class LineNumberArea final : public QWidget {
		public:
			explicit LineNumberArea(ShaderCodeEditor* editor)
				: QWidget(editor), mEditor(editor) {
			}

			QSize sizeHint() const override {
				return QSize(mEditor->LineNumberAreaWidth(), 0);
			}

		protected:
			void paintEvent(QPaintEvent* event) override {
				mEditor->PaintLineNumbers(event);
			}

		private:
			ShaderCodeEditor* mEditor = nullptr;
		};

	} // namespace

	ShaderCodeEditor::ShaderCodeEditor(QWidget* parent)
		: QPlainTextEdit(parent) {
		setReadOnly(true);
		setLineWrapMode(QPlainTextEdit::NoWrap);
		setFrameShape(QFrame::NoFrame);
		setUndoRedoEnabled(false);

		QFont codeFont(QStringLiteral("Consolas"));
		codeFont.setStyleHint(QFont::Monospace);
		codeFont.setFixedPitch(true);
		codeFont.setPointSize(10);
		setFont(codeFont);
		setTabStopDistance(4.0 * fontMetrics().horizontalAdvance(QLatin1Char(' ')));
		document()->setDocumentMargin(6);

		setStyleSheet(QStringLiteral(R"(
			QPlainTextEdit {
				background: #1e1f23;
				color: #d4d4d4;
				selection-background-color: #264f78;
				selection-color: #ffffff;
				border: none;
			}
			QScrollBar:vertical { background: #1a1a1d; width: 12px; margin: 0; }
			QScrollBar::handle:vertical { background: #4a4d52; min-height: 24px; border-radius: 4px; }
			QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
			QScrollBar:horizontal { background: #1a1a1d; height: 12px; margin: 0; }
			QScrollBar::handle:horizontal { background: #4a4d52; min-width: 24px; border-radius: 4px; }
			QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
		)"));

		mLineNumberArea = new LineNumberArea(this);
		connect(this, &QPlainTextEdit::blockCountChanged,
			this, &ShaderCodeEditor::UpdateLineNumberAreaWidth);
		connect(this, &QPlainTextEdit::updateRequest,
			this, &ShaderCodeEditor::UpdateLineNumberArea);
		UpdateLineNumberAreaWidth(0);
	}

	void ShaderCodeEditor::SetSource(const QString& source) {
		setPlainText(source);
		document()->setModified(false);
		moveCursor(QTextCursor::Start);
		verticalScrollBar()->setValue(verticalScrollBar()->minimum());
		horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
		mResetScrollOnShow = true;
	}

	void ShaderCodeEditor::SetEditable(bool editable) {
		setReadOnly(!editable);
		setUndoRedoEnabled(editable);
	}

	int ShaderCodeEditor::LineNumberAreaWidth() const {
		const int digits = std::max(2, QString::number(std::max(1, blockCount())).size());
		return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
	}

	void ShaderCodeEditor::UpdateLineNumberAreaWidth(int newBlockCount) {
		Q_UNUSED(newBlockCount);
		setViewportMargins(LineNumberAreaWidth(), 0, 0, 0);
	}

	void ShaderCodeEditor::UpdateLineNumberArea(const QRect& rect, int dy) {
		if (dy != 0) {
			mLineNumberArea->scroll(0, dy);
		}
		else {
			mLineNumberArea->update(0, rect.y(), mLineNumberArea->width(), rect.height());
		}
		if (rect.contains(viewport()->rect())) {
			UpdateLineNumberAreaWidth(0);
		}
	}

	void ShaderCodeEditor::resizeEvent(QResizeEvent* event) {
		QPlainTextEdit::resizeEvent(event);

		const QRect cr = contentsRect();
		mLineNumberArea->setGeometry(
			QRect(cr.left(), cr.top(), LineNumberAreaWidth(), cr.height()));
	}

	void ShaderCodeEditor::showEvent(QShowEvent* event) {
		QPlainTextEdit::showEvent(event);
		if (!mResetScrollOnShow) {
			return;
		}
		mResetScrollOnShow = false;

		// 第一次显示时文档布局/滚动条范围才最终确定；延后一拍再回到第一行。
		QTimer::singleShot(0, this, [this]() {
			moveCursor(QTextCursor::Start);
			verticalScrollBar()->setValue(verticalScrollBar()->minimum());
			horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
		});
	}

	void ShaderCodeEditor::PaintLineNumbers(QPaintEvent* event) {
		QPainter painter(mLineNumberArea);
		painter.fillRect(event->rect(), QColor(QStringLiteral("#252529")));

		QTextBlock block = firstVisibleBlock();
		int blockNumber = block.blockNumber();
		int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
		int bottom = top + qRound(blockBoundingRect(block).height());

		painter.setPen(QColor(QStringLiteral("#6f7378")));
		const int lineHeight = fontMetrics().height();
		while (block.isValid() && top <= event->rect().bottom()) {
			if (block.isVisible() && bottom >= event->rect().top()) {
				painter.drawText(
					0, top, mLineNumberArea->width() - 6, lineHeight,
					Qt::AlignRight, QString::number(blockNumber + 1));
			}
			block = block.next();
			top = bottom;
			bottom = top + qRound(blockBoundingRect(block).height());
			++blockNumber;
		}
	}

} // namespace MOON
