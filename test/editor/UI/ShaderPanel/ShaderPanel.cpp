#include "ShaderPanel.h"

#include "DefaultMaterialShaders.h"
#include "GlslHighlighter.h"
#include "ShaderCodeEditor.h"

#include "core/MaterialCatalog.h"
#include "core/MaterialShaderLibrary.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
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
			mSelf->setMinimumHeight(220);
			mSelf->setStyleSheet(QStringLiteral(R"(
				QWidget#ShaderInfoBar { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
				QWidget#SubHeader { background: #2b2b2e; border-bottom: 1px solid #1b1b1d; }
				QLabel#ShaderInfo { color: #e0e0e0; font-weight: 600; }
				QLabel#ShaderMeta { color: #7d8288; }
				QLabel#ShaderStats { color: #7d8288; }
				QLabel#SubTitle { color: #d8dbe0; font-weight: 600; }
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

			mMaterialLabel = new QLabel(
				QString::fromUtf8(ShaderSources::kDefaultMaterialName), infoBar);
			mMaterialLabel->setObjectName(QStringLiteral("ShaderInfo"));

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

			infoLayout->addWidget(mMaterialLabel);
			infoLayout->addWidget(meta);
			infoLayout->addStretch(1);
			infoLayout->addWidget(mStats);
			infoLayout->addWidget(mCopy);

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
			mCompileButton = new QPushButton(QStringLiteral("Compile"), materialHeader);
			mCompileButton->setObjectName(QStringLiteral("ShaderCopy"));
			mCompileButton->setCursor(Qt::PointingHandCursor);
			mCompileButton->setToolTip(
				QStringLiteral("Run matc on the edited .mat and refresh GLSL"));
			mMaterialCopy = new QPushButton(QStringLiteral("Copy"), materialHeader);
			mMaterialCopy->setObjectName(QStringLiteral("ShaderCopy"));
			mMaterialCopy->setCursor(Qt::PointingHandCursor);
			mMaterialCopy->setToolTip(QStringLiteral("Copy the original .mat source"));

			materialHeaderLayout->addWidget(materialTitle);
			materialHeaderLayout->addStretch(1);
			materialHeaderLayout->addWidget(mMaterialStats);
			materialHeaderLayout->addWidget(mCompileButton);
			materialHeaderLayout->addWidget(mMaterialCopy);

			mCompileStatus = new QLabel(QStringLiteral("Ready"), materialPane);
			mCompileStatus->setObjectName(QStringLiteral("ShaderStats"));
			mCompileStatus->setContentsMargins(8, 0, 8, 2);

			mMaterial = CreateEditor(materialPane, true);
			mMaterial->SetEditable(true);
			materialLayout->addWidget(materialHeader);
			materialLayout->addWidget(mCompileStatus);
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
			codeSplitter->setSizes({ 300, 430 });

			layout->addWidget(infoBar);
			layout->addWidget(codeSplitter, 1);

			auto* outer = new QVBoxLayout(mSelf);
			outer->setContentsMargins(0, 0, 0, 0);
			outer->setSpacing(0);
			outer->addWidget(content);

			mCompute->SetSource(
				QString::fromUtf8(ShaderSources::kDefaultMaterialComputeNote).trimmed());

			const auto& assets = MaterialAssets();
			mMaterialIndex = DefaultMaterialIndex();
			if (mMaterialIndex >= 0 && mMaterialIndex < static_cast<int>(assets.size())) {
				const MaterialAsset& asset = assets[mMaterialIndex];
				mLibrary = std::make_unique<MaterialShaderLibrary>(asset.data, asset.size);
				mMaterialLabel->setText(QString::fromLatin1(asset.displayName));
				mMaterial->SetSource(LoadShaderResource(asset.matResource).trimmed());
			}
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

			mMatcProcess = new QProcess(mSelf);
			QObject::connect(mMatcProcess,
				QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
				mSelf, [this](int exitCode, QProcess::ExitStatus status) {
					onCompileFinished(exitCode, status);
				});
			QObject::connect(mMatcProcess, &QProcess::errorOccurred,
				mSelf, [this](QProcess::ProcessError) {
					if (mMatcProcess->error() == QProcess::FailedToStart) {
						mCompileStatus->setText(QStringLiteral("matc could not be started"));
						mCompileButton->setEnabled(true);
						mMaterial->SetEditable(true);
						delete mTempDir;
						mTempDir = nullptr;
					}
				});
			QObject::connect(mCompileButton, &QPushButton::clicked,
				mSelf, [this]() { compileMaterial(); });
			QObject::connect(mMaterial->document(), &QTextDocument::modificationChanged,
				mSelf, [this](bool modified) {
					if (modified && mCompileButton->isEnabled()) {
						mCompileStatus->setText(
							QStringLiteral("Modified - press Compile to run matc"));
					}
				});

			mCompileStatus->setText(QStringLiteral("Ready - edit .mat and press Compile"));

			applyVariant(resolveDefaultVariant());

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

		void SetVariantKey(int key) {
			applyVariant(key);
			if (mUsingCompiledPackage && key != mCompiledVariantKey) {
				mCompileStatus->setText(
					QStringLiteral("Variant changed - press Compile to regenerate"));
			}
		}

		QString findMatcExecutable() const {
			const QString appDir = QCoreApplication::applicationDirPath();
			QStringList candidates;
			candidates << appDir + QStringLiteral("/matc.exe");
			candidates << appDir + QStringLiteral("/matc");
#ifdef TEST_MATC_PATH
			candidates << QString::fromUtf8(TEST_MATC_PATH);
#endif
			for (const QString& candidate : candidates) {
				if (QFileInfo::exists(candidate)) {
					return candidate;
				}
			}
			return {};
		}

		QString variantFilter() const {
			// matc --variant-filter 接收“要过滤掉”的 feature 名称。
			QStringList filters;
			if ((mCurrentVariant & 0x01) == 0) filters << QStringLiteral("directionalLighting");
			if ((mCurrentVariant & 0x02) == 0) filters << QStringLiteral("dynamicLighting");
			if ((mCurrentVariant & 0x04) == 0) filters << QStringLiteral("shadowReceiver");
			if ((mCurrentVariant & 0x08) == 0) filters << QStringLiteral("skinning");
			// 0x10 是 depth variant，matc 的 variant-filter 没有 depth 项。
			if ((mCurrentVariant & 0x20) == 0) filters << QStringLiteral("fog");
			if ((mCurrentVariant & 0x40) == 0) filters << QStringLiteral("vsm");
			if ((mCurrentVariant & 0x80) == 0) filters << QStringLiteral("stereo");
			return filters.join(QLatin1Char(','));
		}

		void compileMaterial() {
			if (!mMatcProcess || mMatcProcess->state() != QProcess::NotRunning) {
				return;
			}
			const QString matc = findMatcExecutable();
			if (matc.isEmpty()) {
				mCompileStatus->setText(
					QStringLiteral("matc not found next to the executable"));
				return;
			}

			delete mTempDir;
			mTempDir = new QTemporaryDir(
				QDir::tempPath() + QStringLiteral("/filament-matc-XXXXXX"));
			if (!mTempDir->isValid()) {
				mCompileStatus->setText(QStringLiteral("Could not create temp directory"));
				return;
			}

			const QString input = mTempDir->filePath(QStringLiteral("material.mat"));
			const QString output = mTempDir->filePath(QStringLiteral("material.filamat"));
			QFile inputFile(input);
			if (!inputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				mCompileStatus->setText(QStringLiteral("Could not write temp .mat"));
				return;
			}
			inputFile.write(mMaterial->toPlainText().toUtf8());
			inputFile.close();
			mCompiledVariantKey = mCurrentVariant;

			mCompileButton->setEnabled(false);
			const QString filter = variantFilter();
			mCompileStatus->setText(filter.isEmpty()
				? QStringLiteral("Compiling all variants with matc...")
				: QStringLiteral("Compiling variant %1 with matc...")
					.arg(MaterialShaderLibrary::VariantName(mCurrentVariant)));
			mMaterial->SetEditable(false);

			QStringList arguments{
				QStringLiteral("-a"), QStringLiteral("opengl"),
				QStringLiteral("-p"), QStringLiteral("desktop"),
				// 生成可读的预处理 GLSL（参数名/宏都保留），而不是
				// spirv-cross 优化后的 a/b/c 临时变量。
				QStringLiteral("-g"),
				QStringLiteral("--no-insert-line-directives") };
			if (!filter.isEmpty()) {
				arguments << QStringLiteral("-V") << filter;
			}
			arguments << QStringLiteral("-o") << output << input;

			mMatcProcess->setProgram(matc);
			mMatcProcess->setWorkingDirectory(mTempDir->path());
			mMatcProcess->setArguments(arguments);
			mMatcProcess->start();
		}

		void onCompileFinished(int exitCode, QProcess::ExitStatus status) {
			mCompileButton->setEnabled(true);
			mMaterial->SetEditable(true);

			const QString stderrText =
				QString::fromLocal8Bit(mMatcProcess->readAllStandardError());
			const QString stdoutText =
				QString::fromLocal8Bit(mMatcProcess->readAllStandardOutput());

			bool success = status == QProcess::NormalExit && exitCode == 0;
			QByteArray package;
			if (success && mTempDir) {
				QFile output(mTempDir->filePath(QStringLiteral("material.filamat")));
				if (output.open(QIODevice::ReadOnly)) {
					package = output.readAll();
				}
				success = !package.isEmpty();
			}

			if (success) {
				auto library = std::make_unique<MaterialShaderLibrary>(
					reinterpret_cast<const uint8_t*>(package.constData()),
					static_cast<size_t>(package.size()));
				if (!library->IsValid()) {
					success = false;
					mCompileStatus->setText(
						QStringLiteral("matc produced an unreadable material package"));
				}
				else {
					mCompiledPackage = std::move(package);
					mLibrary = std::move(library);
					mUsingCompiledPackage = true;
					mCompiledVariantKey = mCurrentVariant;
					applyVariant(mCurrentVariant);
					mMaterialStats->setText(
						QStringLiteral("%1 lines").arg(mMaterial->blockCount()));
					mMaterial->document()->setModified(false);
					mCompileStatus->setText(
						QStringLiteral("Compiled %1 - source generated by matc")
							.arg(MaterialShaderLibrary::VariantName(mCurrentVariant)));
				}
			}

			if (!success && mCompileStatus->text().isEmpty()) {
				mCompileStatus->setText(QStringLiteral("matc failed"));
			}
			if (!success) {
				mCompileStatus->setText(QStringLiteral("matc failed - see log"));
				const QString details = (stderrText + stdoutText).trimmed();
				if (!details.isEmpty()) {
					qWarning().noquote()
						<< "matc failed:\n" << details.right(2000);
				}
			}

			delete mTempDir;
			mTempDir = nullptr;
		}

		void SetMaterialIndex(int index) {
			const auto& assets = MaterialAssets();
			if (index < 0 || index >= static_cast<int>(assets.size())) {
				return;
			}
			if (index == mMaterialIndex && mLibrary) {
				return;
			}

			mMaterialIndex = index;
			const MaterialAsset& asset = assets[index];
			mCompiledPackage.clear();
			mUsingCompiledPackage = false;
			mCompiledVariantKey = -1;
			mLibrary = std::make_unique<MaterialShaderLibrary>(asset.data, asset.size);
			mMaterialLabel->setText(QString::fromLatin1(asset.displayName));
			mMaterial->SetSource(LoadShaderResource(asset.matResource).trimmed());
			mMaterialStats->setText(QStringLiteral("%1 lines").arg(mMaterial->blockCount()));
			if (mCompileStatus) {
				mCompileStatus->setText(
					QStringLiteral("Loaded - edit .mat and press Compile"));
			}

			applyVariant(mCurrentVariant);
		}

		ShaderCodeEditor* CreateEditor(QWidget* parent, bool materialSyntax) {
			auto* editor = new ShaderCodeEditor(parent);
			new GlslHighlighter(editor->document(), materialSyntax);
			return editor;
		}

		int resolveDefaultVariant() const {
			const int preferred = MaterialShaderLibrary::DefaultVariant();
			if (!mLibrary) {
				return preferred;
			}
			if (mLibrary->HasVertex(preferred) && mLibrary->HasFragment(preferred)) {
				return preferred;
			}
			const std::vector<int> variants = mLibrary->AvailableVariants();
			return variants.empty() ? preferred : variants.front();
		}

		void applyVariant(int key) {
			mCurrentVariant = key & 0xFF;

			QString vertex;
			QString fragment;
			QString compute;
			if (mLibrary) {
				vertex = mLibrary->VertexSource(mCurrentVariant);
				fragment = mLibrary->FragmentSource(mCurrentVariant);
				compute = mLibrary->ComputeSource(mCurrentVariant);
			}
			mVertexAvailable = !vertex.isEmpty();
			mFragmentAvailable = !fragment.isEmpty();
			mComputeAvailable = !compute.isEmpty();

			mVertex->SetSource(mVertexAvailable
				? vertex
				: missingSourceMessage(QStringLiteral("vertex"), mCurrentVariant));
			mFragment->SetSource(mFragmentAvailable
				? fragment
				: missingSourceMessage(QStringLiteral("fragment"), mCurrentVariant));
			mCompute->SetSource(mComputeAvailable
				? compute
				: QString::fromUtf8(ShaderSources::kDefaultMaterialComputeNote).trimmed());

			updateInfo();
		}

		QString missingSourceMessage(const QString& stage, int key) const {
			return QStringLiteral(
				"// No compiled %1 shader for variant %2.\n"
				"// This material only contains a subset of feature combinations.\n"
				"// Change the Variant Features in the Material panel.\n")
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
				mStats->setText(mComputeAvailable
					? QStringLiteral("%1 lines").arg(mCompute->blockCount())
					: QStringLiteral("no compute stage"));
				break;
			default:
				mStats->clear();
				break;
			}
			mCopy->setEnabled(!editor->document()->isEmpty());
			mMaterialStats->setText(QStringLiteral("%1 lines").arg(mMaterial->blockCount()));
		}

		~ShaderPanelInternal() {
			delete mTempDir;
		}

	private:
		ShaderPanel* mSelf = nullptr;
		std::unique_ptr<MaterialShaderLibrary> mLibrary;
		QTabWidget* mTabs = nullptr;
		ShaderCodeEditor* mVertex = nullptr;
		ShaderCodeEditor* mFragment = nullptr;
		ShaderCodeEditor* mCompute = nullptr;
		ShaderCodeEditor* mMaterial = nullptr;
		QLabel* mMaterialLabel = nullptr;
		QLabel* mStats = nullptr;
		QLabel* mMaterialStats = nullptr;
		QLabel* mCompileStatus = nullptr;
		QPushButton* mCopy = nullptr;
		QPushButton* mMaterialCopy = nullptr;
		QPushButton* mCompileButton = nullptr;
		QProcess* mMatcProcess = nullptr;
		QTemporaryDir* mTempDir = nullptr;
		QByteArray mCompiledPackage;
		int mMaterialIndex = 0;
		int mCurrentVariant = 0;
		int mCompiledVariantKey = -1;
		bool mUsingCompiledPackage = false;
		bool mVertexAvailable = false;
		bool mFragmentAvailable = false;
		bool mComputeAvailable = false;
	};

	ShaderPanel::ShaderPanel(QWidget* parent)
		: QWidget(parent), mInternal(new ShaderPanelInternal(this)) {
		mInternal->setUp();
	}

	ShaderPanel::~ShaderPanel() {
		delete mInternal;
	}

	void ShaderPanel::SetVariantKey(int key) {
		if (mInternal) {
			mInternal->SetVariantKey(key);
		}
	}

	void ShaderPanel::SetMaterialIndex(int index) {
		if (mInternal) {
			mInternal->SetMaterialIndex(index);
		}
	}

} // namespace MOON
