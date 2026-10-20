#include "GlslHighlighter.h"

#include <QFont>

namespace MOON {

	GlslHighlighter::GlslHighlighter(QTextDocument* parent)
		: QSyntaxHighlighter(parent) {
		mCommentFormat.setForeground(QColor(QStringLiteral("#6a9955")));
		mCommentFormat.setFontItalic(true);
		mPreprocessorFormat.setForeground(QColor(QStringLiteral("#9cdcfe")));

		AddRule(QStringLiteral(
			R"(\b(if|else|for|while|do|switch|case|default|return|break|continue|discard|)"
			R"(struct|uniform|buffer|shared|layout|precision|invariant|const|in|out|inout|)"
			R"(flat|smooth|noperspective|centroid|highp|mediump|lowp)\b)"),
			QColor(QStringLiteral("#c586c0")), true);

		AddRule(QStringLiteral(
			R"(\b(void|bool|int|uint|float|double|vec2|vec3|vec4|ivec2|ivec3|ivec4|)"
			R"(uvec2|uvec3|uvec4|bvec2|bvec3|bvec4|mat2|mat3|mat4|mat2x2|mat2x3|)"
			R"(mat2x4|mat3x2|mat3x3|mat3x4|mat4x2|mat4x3|mat4x4|sampler2D|sampler3D|)"
			R"(samplerCube|sampler2DArray|samplerCubeArray|sampler2DShadow|)"
			R"(samplerCubeShadow|isampler2D|usampler2D|image2D|image3D|atomic_uint)\b)"),
			QColor(QStringLiteral("#4ec9b0")));

		AddRule(QStringLiteral(
			R"(\b(gl_Position|gl_PointSize|gl_FragCoord|gl_FrontFacing|gl_FragColor|)"
			R"(gl_FragDepth|gl_VertexID|gl_InstanceID|texture|textureLod|textureGrad|)"
			R"(texelFetch|textureSize|dFdx|dFdy|fwidth|mix|clamp|saturate|dot|cross|)"
			R"(normalize|length|distance|pow|exp2|exp|log2|log|sqrt|inversesqrt|sin|cos|)"
			R"(tan|asin|acos|atan|min|max|abs|sign|floor|ceil|fract|mod|step|smoothstep|)"
			R"(reflect|refract|faceforward|transpose|inverse|determinant)\b)"),
			QColor(QStringLiteral("#dcdcaa")));

		AddRule(QStringLiteral(
			R"(\b(0[xX][0-9a-fA-F]+|\d+\.\d*([eE][+-]?\d+)?|\d+([eE][+-]?\d+)?)[fFuU]?\b)"),
			QColor(QStringLiteral("#b5cea8")));
	}

	void GlslHighlighter::AddRule(const QString& pattern, const QColor& color, bool bold) {
		QTextCharFormat format;
		format.setForeground(color);
		if (bold) {
			format.setFontWeight(QFont::Bold);
		}
		mRules.push_back({ QRegularExpression(pattern), format });
	}

	void GlslHighlighter::highlightBlock(const QString& text) {
		setCurrentBlockState(0);

		for (const Rule& rule : mRules) {
			auto matches = rule.pattern.globalMatch(text);
			while (matches.hasNext()) {
				const QRegularExpressionMatch match = matches.next();
				setFormat(match.capturedStart(), match.capturedLength(), rule.format);
			}
		}

		// 单行注释（块注释稍后覆盖重叠部分）
		const int lineComment = text.indexOf(QStringLiteral("//"));
		if (lineComment >= 0) {
			setFormat(lineComment, text.length() - lineComment, mCommentFormat);
		}

		// 多行注释
		int startIndex = 0;
		if (previousBlockState() != 1) {
			startIndex = text.indexOf(QStringLiteral("/*"));
		}
		while (startIndex >= 0) {
			const int endIndex = text.indexOf(QStringLiteral("*/"), startIndex + 2);
			int commentLength = 0;
			if (endIndex == -1) {
				setCurrentBlockState(1);
				commentLength = text.length() - startIndex;
			}
			else {
				commentLength = endIndex - startIndex + 2;
			}
			setFormat(startIndex, commentLength, mCommentFormat);
			if (endIndex == -1) {
				break;
			}
			startIndex = text.indexOf(QStringLiteral("/*"), startIndex + commentLength);
		}

		// 预处理指令整行着色
		const QRegularExpressionMatch preprocessor =
			QRegularExpression(QStringLiteral(R"(^\s*#.*)")).match(text);
		if (preprocessor.hasMatch()) {
			setFormat(preprocessor.capturedStart(), preprocessor.capturedLength(),
				mPreprocessorFormat);
		}
	}

}
