#include "MaterialShaderLibrary.h"

#include <filaflat/ChunkContainer.h>
#include <filaflat/DictionaryReader.h>
#include <filaflat/MaterialChunk.h>

#include <filament/MaterialChunkType.h>

#include <private/filament/Variant.h>

#include <backend/DriverEnums.h>

#include <QDebug>
#include <QStringList>

#include <map>
#include <set>

namespace MOON {
	namespace {
		using filament::backend::ShaderModel;
		using filament::backend::ShaderStage;
		using filament::Variant;
	}

	struct MaterialShaderLibrary::Impl {
		std::unique_ptr<filaflat::ChunkContainer> container;
		std::unique_ptr<filaflat::MaterialChunk> material;
		filaflat::BlobDictionary dictionary;
		std::map<int, QString> vertex;
		std::map<int, QString> fragment;
		bool valid = false;

		Impl(const uint8_t* packageData, size_t packageSize) {
			container = std::make_unique<filaflat::ChunkContainer>(
				packageData, packageSize);
			if (!container->parse()) {
				qWarning() << "MaterialShaderLibrary: cannot parse material package";
				return;
			}
			if (!filaflat::DictionaryReader::unflatten(
				*container, filamat::ChunkType::DictionaryText, dictionary)) {
				qWarning() << "MaterialShaderLibrary: cannot read shader dictionary";
				return;
			}
			material = std::make_unique<filaflat::MaterialChunk>(*container);
			if (!material->initialize(filamat::ChunkType::MaterialGlsl)) {
				qWarning() << "MaterialShaderLibrary: no GLSL chunk in package";
				return;
			}

			material->visitShaders([this](ShaderModel model, Variant variant, ShaderStage stage) {
				if (model != ShaderModel::DESKTOP) {
					return;
				}
				if (stage != ShaderStage::VERTEX && stage != ShaderStage::FRAGMENT) {
					return;
				}

				const Variant filtered = stage == ShaderStage::VERTEX
					? Variant::filterVariantVertex(variant)
					: Variant::filterVariantFragment(variant);

				filaflat::ShaderContent content;
				if (!material->getShader(content, dictionary, model, filtered, stage)) {
					return;
				}
				if (content.empty()) {
					return;
				}
				QString source = QString::fromUtf8(
					reinterpret_cast<const char*>(content.data()),
					static_cast<int>(content.size()));
				if (stage == ShaderStage::VERTEX) {
					vertex[filtered.key] = std::move(source);
				}
				else {
					fragment[filtered.key] = std::move(source);
				}
			});

			valid = !vertex.empty() && !fragment.empty();
			qInfo() << "MaterialShaderLibrary: variants" << vertex.size()
				<< "vertex" << fragment.size() << "fragment";
		}

		QString sourceFor(const std::map<int, QString>& sources, int key, bool vertexStage) const {
			const Variant filtered = vertexStage
				? Variant::filterVariantVertex(Variant(static_cast<uint8_t>(key)))
				: Variant::filterVariantFragment(Variant(static_cast<uint8_t>(key)));
			const auto it = sources.find(filtered.key);
			return it == sources.end() ? QString() : it->second;
		}
	};

	MaterialShaderLibrary::MaterialShaderLibrary(
		const uint8_t* packageData, size_t packageSize)
		: mImpl(std::make_unique<Impl>(packageData, packageSize)) {
	}

	MaterialShaderLibrary::~MaterialShaderLibrary() = default;

	bool MaterialShaderLibrary::IsValid() const {
		return mImpl && mImpl->valid;
	}

	QString MaterialShaderLibrary::VertexSource(int variantKey) const {
		return mImpl ? mImpl->sourceFor(mImpl->vertex, variantKey, true) : QString();
	}

	QString MaterialShaderLibrary::FragmentSource(int variantKey) const {
		return mImpl ? mImpl->sourceFor(mImpl->fragment, variantKey, false) : QString();
	}

	bool MaterialShaderLibrary::HasVertex(int variantKey) const {
		return !VertexSource(variantKey).isEmpty();
	}

	bool MaterialShaderLibrary::HasFragment(int variantKey) const {
		return !FragmentSource(variantKey).isEmpty();
	}

	std::vector<int> MaterialShaderLibrary::AvailableVariants() const {
		std::set<int> keys;
		if (mImpl) {
			for (const auto& entry : mImpl->vertex) {
				keys.insert(entry.first);
			}
			for (const auto& entry : mImpl->fragment) {
				keys.insert(entry.first);
			}
		}
		return std::vector<int>(keys.begin(), keys.end());
	}

	const std::vector<ShaderFeature>& MaterialShaderLibrary::Features() {
		static const std::vector<ShaderFeature> features = {
			{ "DIR", "Directional", "Directional light",              0x01 },
			{ "DYN", "Dynamic",     "Dynamic light (point/spot/area)", 0x02 },
			{ "SRE", "Shadows",     "Shadow receiver",                 0x04 },
			{ "SKN", "Skinning",    "GPU skinning / morphing",         0x08 },
			{ "DEP", "Depth",       "Depth-only pass",                 0x10 },
			{ "FOG", "Fog",         "Fog",                             0x20 },
			{ "VSM", "Variance",    "Variance shadow maps",            0x40 },
			{ "STE", "Stereo",      "Instanced stereo",                0x80 },
		};
		return features;
	}

	QString MaterialShaderLibrary::VariantName(int variantKey) {
		const int key = variantKey & 0xFF;
		QStringList parts;
		for (const ShaderFeature& feature : Features()) {
			if (key & feature.bit) {
				parts << QString::fromLatin1(feature.name);
			}
		}

		const QString hex = QString::number(key, 16).rightJustified(2, QLatin1Char('0')).toUpper();
		return QStringLiteral("0x%1 (%2)")
			.arg(hex)
			.arg(parts.isEmpty() ? QStringLiteral("none") : parts.join(QLatin1Char('|')));
	}

} // namespace MOON
