#pragma once
#include <QColor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class QTextDocument;

namespace MOON {

	// 轻量 GLSL 语法高亮：关键字/类型/内建函数/数字/注释/预处理指令。
	class GlslHighlighter : public QSyntaxHighlighter
	{
	public:
		explicit GlslHighlighter(QTextDocument* parent);

	protected:
		void highlightBlock(const QString& text) override;

	private:
		struct Rule {
			QRegularExpression pattern;
			QTextCharFormat format;
		};

		void AddRule(const QString& pattern, const QColor& color, bool bold = false);

		QVector<Rule> mRules;
		QTextCharFormat mCommentFormat;
		QTextCharFormat mPreprocessorFormat;
	};

}
