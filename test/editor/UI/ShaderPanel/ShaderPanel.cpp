#include "ShaderPanel.h"

#include "DefaultMaterialShaders.h"
#include "GlslHighlighter.h"
#include "ShaderCodeEditor.h"

#include "editor/UI/DockWidgetTitleBar.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace MOON {
	namespace {

		QString LoadShaderResource(const char* path) {
			QFile file(QString::fromLatin1(path));
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				return QStringLiteral("// Failed to load shader resource: %1\n")
					.arg(QString::fromLatin1(path));
			}
			return QString::fromUtf8(file.readAll());
		}

	} // namespace

	class ShaderPanel::ShaderPanelInternal {
	public:
		explicit ShaderPanelInternal(ShaderPanel* self) : mSelf(self) {}

		void setUp() {
			mSelf->setObjectName(QStringLiteral("ShaderPanel"));
			mSelf->setAllowedAreas(Qt::AllDockWidgetAreas);
			mSelf->setMinimumHeight(220);
			mSelf->setTitleBarWidget(new DockWidgetTitleBar(mSelf));
			mSelf->setStyleSheet(QStringLiteral(R"(
				QWidget#ShaderInfoBar { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
				QLabel#ShaderInfo { color: #e0e0e0; font-weight: 600; }
				QLabel#ShaderMeta { color: #7d8288; }
				QLabel#ShaderStats { color: #7d8288; }
				QPushButton#ShaderCopy {
					color: #d7dae0;
					background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
						stop:0 #5a5a60, stop:0.15 #323235, stop:0.9 #323235, stop:1 #222224);
					border: 1px solid #1b1b1d;
					border-radius: 3px;
					padding: 2px 10px;
				}
				QPushButton#ShaderCopy:hover { border-color: #268bd2; }
				QPushButton#ShaderCopy:disabled { color: #6f7378; }
				QTabWidget::pane { border: none; background: #1e1f23; }
				QTabBar::tab {
					background: #2b2b2e; color: #aeb3b9;
					padding: 4px 14px; border: none;
				}
				QTabBar::tab:selected {
					background: #1e1f23; color: #e8e8e8;
					border-top: 1px solid #268bd2;
				}
				QTabBar::tab:hover { color: #ffffff; }
			)"));

			auto* content = new QWidget(mSelf);
			auto* layout = new QVBoxLayout(content);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);

			auto* infoBar = new QWidget(content);
			infoBar->setObjectName(QStringLiteral("ShaderInfoBar"));
			auto* infoLayout = new QHBoxLayout(infoBar);
			infoLayout->setContentsMargins(8, 2, 8, 2);
			infoLayout->setSpacing(10);

			auto* material = new QLabel(
				QString::fromUtf8(ShaderSources::kDefaultMaterialName), infoBar);
			material->setObjectName(QStringLiteral("ShaderInfo"));

			auto* meta = new QLabel(
				QStringLiteral("%1 · %2 · read-only")
					.arg(QString::fromUtf8(ShaderSources::kDefaultMaterialApi))
					.arg(QString::fromUtf8(ShaderSources::kDefaultMaterialVariant)),
				infoBar);
			meta->setObjectName(QStringLiteral("ShaderMeta"));

			mStats = new QLabel(infoBar);
			mStats->setObjectName(QStringLiteral("ShaderStats"));

			mCopy = new QPushButton(QStringLiteral("Copy"), infoBar);
			mCopy->setObjectName(QStringLiteral("ShaderCopy"));
			mCopy->setCursor(Qt::PointingHandCursor);
			mCopy->setToolTip(QStringLiteral("Copy the current shader source"));

			infoLayout->addWidget(material);
			infoLayout->addWidget(meta);
			infoLayout->addStretch(1);
			infoLayout->addWidget(mStats);
			infoLayout->addWidget(mCopy);

			mTabs = new QTabWidget(content);
			mTabs->setDocumentMode(true);

			mVertex = CreateEditor(mTabs,
				LoadShaderResource(ShaderSources::kDefaultMaterialVertexResource));
			mFragment = CreateEditor(mTabs,
				LoadShaderResource(ShaderSources::kDefaultMaterialFragmentResource));
			mCompute = CreateEditor(mTabs,
				QString::fromUtf8(ShaderSources::kDefaultMaterialComputeNote));

			mTabs->addTab(mVertex, QStringLiteral("Vertex"));
			mTabs->addTab(mFragment, QStringLiteral("Fragment"));
			mTabs->addTab(mCompute, QStringLiteral("Compute"));

			layout->addWidget(infoBar);
			layout->addWidget(mTabs, 1);

			mSelf->setWidget(content);

			QObject::connect(mTabs, &QTabWidget::currentChanged, mSelf, [this](int) {
				updateInfo();
			});
			QObject::connect(mCopy, &QPushButton::clicked, mSelf, [this]() {
				if (ShaderCodeEditor* editor = currentEditor()) {
					QApplication::clipboard()->setText(editor->toPlainText());
				}
			});

			updateInfo();

			// 首次布局完成后强制回到 Vertex 页和第一行；QPlainTextEdit 在
			// 文档布局/滚动条范围更新后可能保留上一次的滚动位置。
			mTabs->setCurrentIndex(0);
			QTimer::singleShot(0, mSelf, [this]() {
				mTabs->setCurrentIndex(0);
				for (ShaderCodeEditor* editor : { mVertex, mFragment, mCompute }) {
					editor->verticalScrollBar()->setValue(
						editor->verticalScrollBar()->minimum());
					editor->horizontalScrollBar()->setValue(
						editor->horizontalScrollBar()->minimum());
				}
				updateInfo();
			});
		}

		ShaderCodeEditor* CreateEditor(QWidget* parent, const QString& source) {
			auto* editor = new ShaderCodeEditor(parent);
			new GlslHighlighter(editor->document());
			editor->SetSource(source.trimmed());
			return editor;
		}

		ShaderCodeEditor* currentEditor() const {
			switch (mTabs->currentIndex()) {
			case 0: return mVertex;
			case 1: return mFragment;
			case 2: return mCompute;
			default: return nullptr;
			}
		}

		void updateInfo() {
			ShaderCodeEditor* editor = currentEditor();
			if (!editor) {
				mStats->clear();
				return;
			}

			const int index = mTabs->currentIndex();
			if (index == 2) {
				mStats->setText(QStringLiteral("no compute stage"));
			}
			else {
				mStats->setText(QStringLiteral("%1 lines").arg(editor->blockCount()));
			}
			mCopy->setEnabled(!editor->document()->isEmpty());
		}

	private:
		ShaderPanel* mSelf = nullptr;
		QTabWidget* mTabs = nullptr;
		ShaderCodeEditor* mVertex = nullptr;
		ShaderCodeEditor* mFragment = nullptr;
		ShaderCodeEditor* mCompute = nullptr;
		QLabel* mStats = nullptr;
		QPushButton* mCopy = nullptr;
	};

	ShaderPanel::ShaderPanel(QWidget* parent)
		: QDockWidget(parent), mInternal(new ShaderPanelInternal(this)) {
		mInternal->setUp();
	}

	ShaderPanel::~ShaderPanel() {
		delete mInternal;
	}

} // namespace MOON
