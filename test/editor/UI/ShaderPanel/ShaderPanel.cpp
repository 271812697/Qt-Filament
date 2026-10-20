#include "ShaderPanel.h"

#include "DefaultMaterialShaders.h"
#include "GlslHighlighter.h"
#include "ShaderCodeEditor.h"

#include "core/MaterialShaderLibrary.h"

#include "Widgets/InviwoCheckBox.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

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
			mSelf->setMinimumHeight(260);
			mSelf->setStyleSheet(QStringLiteral(R"(
				QWidget#ShaderInfoBar { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
				QWidget#SubHeader { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
				QLabel#ShaderInfo { color: #e0e0e0; font-weight: 600; }
				QLabel#ShaderMeta { color: #7d8288; }
				QLabel#ShaderStats { color: #7d8288; }
				QLabel#SubTitle { color: #d8dbe0; font-weight: 600; }
				QWidget#VariantSide { background: #242428; border-right: 1px solid #35353a; }
				QLabel#PanelCaption { color: #7d8288; font-size: 10px; }
				QLabel#VariantValue { color: #dcdcaa; font-weight: 600; }
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
				QSplitter::handle { background: #35353a; }
				QSplitter::handle:horizontal { width: 1px; }
			)"));

			auto* content = new QWidget(mSelf);
			auto* layout = new QVBoxLayout(content);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);

			// 顶部材质/API 信息
			auto* infoBar = new QWidget(content);
			infoBar->setObjectName(QStringLiteral("ShaderInfoBar"));
			auto* infoLayout = new QHBoxLayout(infoBar);
			infoLayout->setContentsMargins(8, 2, 8, 2);
			infoLayout->setSpacing(10);

			auto* material = new QLabel(
				QString::fromUtf8(ShaderSources::kDefaultMaterialName), infoBar);
			material->setObjectName(QStringLiteral("ShaderInfo"));

			auto* meta = new QLabel(
				QStringLiteral("%1 · read-only")
					.arg(QString::fromUtf8(ShaderSources::kDefaultMaterialApi)),
				infoBar);
			meta->setObjectName(QStringLiteral("ShaderMeta"));

			mStats = new QLabel(infoBar);
			mStats->setObjectName(QStringLiteral("ShaderStats"));

			mCopy = new QPushButton(QStringLiteral("Copy GLSL"), infoBar);
			mCopy->setObjectName(QStringLiteral("ShaderCopy"));
			mCopy->setCursor(Qt::PointingHandCursor);
			mCopy->setToolTip(QStringLiteral("Copy the active GLSL tab"));

			infoLayout->addWidget(material);
			infoLayout->addWidget(meta);
			infoLayout->addStretch(1);
			infoLayout->addWidget(mStats);
			infoLayout->addWidget(mCopy);

			// 左侧 variant feature，勾选框与 Material 面板的 Inviwo 方框保持一致
			auto* side = new QWidget(content);
			side->setObjectName(QStringLiteral("VariantSide"));
			side->setFixedWidth(200);
			auto* sideLayout = new QVBoxLayout(side);
			sideLayout->setContentsMargins(9, 6, 9, 6);
			sideLayout->setSpacing(3);

			auto* featuresCaption = new QLabel(QStringLiteral("VARIANT FEATURES"), side);
			featuresCaption->setObjectName(QStringLiteral("PanelCaption"));
			sideLayout->addWidget(featuresCaption);

			for (const ShaderFeature& feature : MaterialShaderLibrary::Features()) {
				auto* row = new QWidget(side);
				auto* rowLayout = new QHBoxLayout(row);
				rowLayout->setContentsMargins(0, 0, 0, 0);
				rowLayout->setSpacing(6);

				const QString labelText = QStringLiteral("%1  %2")
					.arg(QString::fromLatin1(feature.name),
						QString::fromLatin1(feature.shortName));
				auto* label = new QLabel(labelText, row);
				label->setStyleSheet(QStringLiteral("color: #c8ccd2;"));

				auto* check = new RectCheckBoxButton(row);
				check->setToolTip(QString::fromLatin1(feature.description));
				check->setAccessibleName(QString::fromLatin1(feature.name));

				rowLayout->addWidget(label, 1);
				rowLayout->addWidget(check, 0, Qt::AlignRight | Qt::AlignVCenter);
				sideLayout->addWidget(row);

				mFeatureChecks.push_back(check);
				QObject::connect(check, &QAbstractButton::toggled, mSelf, [this](bool) {
					if (!mSyncing) {
						applyCheckedVariant();
					}
				});
			}

			sideLayout->addSpacing(4);
			auto* variantCaption = new QLabel(QStringLiteral("VARIANT"), side);
			variantCaption->setObjectName(QStringLiteral("PanelCaption"));
			sideLayout->addWidget(variantCaption);

			mVariantValue = new QLabel(side);
			mVariantValue->setObjectName(QStringLiteral("VariantValue"));
			mVariantValue->setWordWrap(true);
			sideLayout->addWidget(mVariantValue);

			sideLayout->addStretch(1);

			// 代码区：左侧 .mat，右侧 GLSL 三个阶段，可拖动分隔
			auto* codeSplitter = new QSplitter(Qt::Horizontal, content);
			codeSplitter->setChildrenCollapsible(false);

			auto* materialPane = new QWidget(codeSplitter);
			auto* materialLayout = new QVBoxLayout(materialPane);
			materialLayout->setContentsMargins(0, 0, 0, 0);
			materialLayout->setSpacing(0);

			auto* materialHeader = new QWidget(materialPane);
			materialHeader->setObjectName(QStringLiteral("SubHeader"));
			auto* materialHeaderLayout = new QHBoxLayout(materialHeader);
			materialHeaderLayout->setContentsMargins(8, 2, 8, 2);
			materialHeaderLayout->setSpacing(8);

			auto* materialTitle = new QLabel(QStringLiteral("Material (.mat)"), materialHeader);
			materialTitle->setObjectName(QStringLiteral("SubTitle"));
			mMaterialStats = new QLabel(materialHeader);
			mMaterialStats->setObjectName(QStringLiteral("ShaderStats"));
			mMaterialCopy = new QPushButton(QStringLiteral("Copy"), materialHeader);
			mMaterialCopy->setObjectName(QStringLiteral("ShaderCopy"));
			mMaterialCopy->setCursor(Qt::PointingHandCursor);
			mMaterialCopy->setToolTip(QStringLiteral("Copy the original .mat source"));

			materialHeaderLayout->addWidget(materialTitle);
			materialHeaderLayout->addStretch(1);
			materialHeaderLayout->addWidget(mMaterialStats);
			materialHeaderLayout->addWidget(mMaterialCopy);

			mMaterial = CreateEditor(materialPane, true);
			materialLayout->addWidget(materialHeader);
			materialLayout->addWidget(mMaterial, 1);

			auto* glslPane = new QWidget(codeSplitter);
			auto* glslLayout = new QVBoxLayout(glslPane);
			glslLayout->setContentsMargins(0, 0, 0, 0);
			glslLayout->setSpacing(0);

			mTabs = new QTabWidget(glslPane);
			mTabs->setDocumentMode(true);
			mVertex = CreateEditor(mTabs, false);
			mFragment = CreateEditor(mTabs, false);
			mCompute = CreateEditor(mTabs, false);
			mTabs->addTab(mVertex, QStringLiteral("Vertex"));
			mTabs->addTab(mFragment, QStringLiteral("Fragment"));
			mTabs->addTab(mCompute, QStringLiteral("Compute"));
			glslLayout->addWidget(mTabs);

			codeSplitter->addWidget(materialPane);
			codeSplitter->addWidget(glslPane);
			codeSplitter->setStretchFactor(0, 2);
			codeSplitter->setStretchFactor(1, 3);
			codeSplitter->setSizes({ 430, 760 });

			auto* body = new QHBoxLayout();
			body->setContentsMargins(0, 0, 0, 0);
			body->setSpacing(0);
			body->addWidget(side);
			body->addWidget(codeSplitter, 1);

			layout->addWidget(infoBar);
			layout->addLayout(body, 1);

			auto* outer = new QVBoxLayout(mSelf);
			outer->setContentsMargins(0, 0, 0, 0);
			outer->setSpacing(0);
			outer->addWidget(content);

			mCompute->SetSource(
				QString::fromUtf8(ShaderSources::kDefaultMaterialComputeNote).trimmed());
			mMaterial->SetSource(LoadShaderResource(
				ShaderSources::kDefaultMaterialMatResource).trimmed());
			mMaterialStats->setText(QStringLiteral("%1 lines").arg(mMaterial->blockCount()));

			QObject::connect(mTabs, &QTabWidget::currentChanged, mSelf, [this](int) {
				updateInfo();
				QTimer::singleShot(0, mSelf, [this]() {
					if (ShaderCodeEditor* editor = currentEditor()) {
						editor->verticalScrollBar()->setValue(
							editor->verticalScrollBar()->minimum());
						editor->horizontalScrollBar()->setValue(
							editor->horizontalScrollBar()->minimum());
					}
				});
			});
			QObject::connect(mCopy, &QPushButton::clicked, mSelf, [this]() {
				if (ShaderCodeEditor* editor = currentEditor()) {
					QApplication::clipboard()->setText(editor->toPlainText());
				}
			});
			QObject::connect(mMaterialCopy, &QPushButton::clicked, mSelf, [this]() {
				QApplication::clipboard()->setText(mMaterial->toPlainText());
			});

			setVariantKey(resolveDefaultVariant());

			// 首次布局后确保默认停在第 1 行。
			QTimer::singleShot(0, mSelf, [this]() {
				mTabs->setCurrentIndex(0);
				for (ShaderCodeEditor* editor : { mVertex, mFragment, mCompute, mMaterial }) {
					editor->verticalScrollBar()->setValue(
						editor->verticalScrollBar()->minimum());
					editor->horizontalScrollBar()->setValue(
						editor->horizontalScrollBar()->minimum());
				}
				updateInfo();
			});
		}

		ShaderCodeEditor* CreateEditor(QWidget* parent, bool materialSyntax) {
			auto* editor = new ShaderCodeEditor(parent);
			new GlslHighlighter(editor->document(), materialSyntax);
			return editor;
		}

		int resolveDefaultVariant() const {
			const int preferred = MaterialShaderLibrary::DefaultVariant();
			if (mLibrary.HasVertex(preferred) && mLibrary.HasFragment(preferred)) {
				return preferred;
			}
			const std::vector<int> variants = mLibrary.AvailableVariants();
			return variants.empty() ? preferred : variants.front();
		}

		void applyCheckedVariant() {
			int key = 0;
			const auto& features = MaterialShaderLibrary::Features();
			for (size_t i = 0; i < mFeatureChecks.size() && i < features.size(); ++i) {
				if (mFeatureChecks[i]->isChecked()) {
					key |= features[i].bit;
				}
			}
			applyVariant(key, false);
		}

		void setVariantKey(int key) {
			applyVariant(key, true);
		}

		void applyVariant(int key, bool syncCheckboxes) {
			mCurrentVariant = key & 0xFF;
			mSyncing = true;
			if (syncCheckboxes) {
				const auto& features = MaterialShaderLibrary::Features();
				for (size_t i = 0; i < mFeatureChecks.size() && i < features.size(); ++i) {
					mFeatureChecks[i]->setChecked((mCurrentVariant & features[i].bit) != 0);
				}
			}
			mSyncing = false;

			const QString vertex = mLibrary.VertexSource(mCurrentVariant);
			const QString fragment = mLibrary.FragmentSource(mCurrentVariant);
			mVertexAvailable = !vertex.isEmpty();
			mFragmentAvailable = !fragment.isEmpty();

			mVertex->SetSource(mVertexAvailable
				? vertex
				: missingSourceMessage(QStringLiteral("vertex"), mCurrentVariant));
			mFragment->SetSource(mFragmentAvailable
				? fragment
				: missingSourceMessage(QStringLiteral("fragment"), mCurrentVariant));
			mVariantValue->setText(MaterialShaderLibrary::VariantName(mCurrentVariant));

			updateInfo();
		}

		QString missingSourceMessage(const QString& stage, int key) const {
			return QStringLiteral(
				"// No compiled %1 shader for variant %2.\n"
				"// aiDefaultMat only contains a subset of feature combinations.\n"
				"// Change the feature checkboxes to a combination that matc compiled.\n")
				.arg(stage)
				.arg(MaterialShaderLibrary::VariantName(key));
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

			switch (mTabs->currentIndex()) {
			case 0:
				mStats->setText(mVertexAvailable
					? QStringLiteral("%1 lines").arg(mVertex->blockCount())
					: QStringLiteral("not compiled"));
				break;
			case 1:
				mStats->setText(mFragmentAvailable
					? QStringLiteral("%1 lines").arg(mFragment->blockCount())
					: QStringLiteral("not compiled"));
				break;
			case 2:
				mStats->setText(QStringLiteral("no compute stage"));
				break;
			default:
				mStats->clear();
				break;
			}
			mCopy->setEnabled(!editor->document()->isEmpty());
			mMaterialStats->setText(QStringLiteral("%1 lines").arg(mMaterial->blockCount()));
		}

	private:
		ShaderPanel* mSelf = nullptr;
		MaterialShaderLibrary mLibrary;
		QTabWidget* mTabs = nullptr;
		ShaderCodeEditor* mVertex = nullptr;
		ShaderCodeEditor* mFragment = nullptr;
		ShaderCodeEditor* mCompute = nullptr;
		ShaderCodeEditor* mMaterial = nullptr;
		QLabel* mVariantValue = nullptr;
		std::vector<RectCheckBoxButton*> mFeatureChecks;
		QLabel* mStats = nullptr;
		QLabel* mMaterialStats = nullptr;
		QPushButton* mCopy = nullptr;
		QPushButton* mMaterialCopy = nullptr;
		int mCurrentVariant = 0;
		bool mSyncing = false;
		bool mVertexAvailable = false;
		bool mFragmentAvailable = false;
	};

	ShaderPanel::ShaderPanel(QWidget* parent)
		: QWidget(parent), mInternal(new ShaderPanelInternal(this)) {
		mInternal->setUp();
	}

	ShaderPanel::~ShaderPanel() {
		delete mInternal;
	}

} // namespace MOON
