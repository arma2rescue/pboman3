#include "extractconfiguration.h"

#include <QJsonDocument>

#include "domain/func.h"
#include "io/diskaccessexception.h"
#include "model/binarysourceutils.h"

namespace pboman3::model::task {
    PackOptions ExtractConfiguration::extractFrom(const PboDocument& document) {
        PackOptions options;

        extractHeaders(document, options);
        extractCompressionRules(document, options);

        return options;
    }

    void ExtractConfiguration::saveTo(const PackOptions& options, const QDir& dest) {
        const QJsonObject json = options.makeJson();
        const QByteArray bytes = QJsonDocument(json).toJson(QJsonDocument::Indented);

        const QString fileName = getConfigFileName(dest);
        QFile file(fileName);
        if (!file.open(QIODeviceBase::WriteOnly | QIODeviceBase::NewOnly)) {
            throw DiskAccessException("Can not access the file. Check if it is used by other processes.", fileName);
        }
        file.write(bytes);
        file.close();
    }

    void ExtractConfiguration::extractHeaders(const PboDocument& document, PackOptions& options) {
        for (const DocumentHeader* header : *document.headers()) {
            options.headers.append(PackHeader(header->name(), header->value()));
        }
    }

    constexpr const char* extensions[] = {"sqf", "sqs", "txt", "xml", "csv"};
    constexpr const char* files[] = {"mission.sqm", "description.ext"};

    void ExtractConfiguration::extractCompressionRules(const PboDocument& document, PackOptions& options) {
        QSet<QString> artifacts;
        artifacts.reserve(10);
        collectValuableArtifacts(document.root(), artifacts);

        for (const QString ext : extensions) {
            if (artifacts.contains(ext)) {
                const QString rule = makeExtensionCompressionRule(ext);
                options.compress.include.append(rule);
            }
        }
        for (const QString file : files) {
            if (const PboNode* node = FindDirectChild(document.root(), file); node) {
                if (IsCompressed(node->binarySource)) {
                    const QString rule = makeFileCompressionRule(file);
                    options.compress.include.append(rule);
                }
            }
        }
    }

    void ExtractConfiguration::collectValuableArtifacts(const PboNode* node, QSet<QString>& results) {
        const QString title = node->title().toLower();
        if (node->nodeType() == PboNodeType::File) {
            const QString ext = GetFileExtension(title);
            results.insert(ext);
        } else {
            for (const PboNode* child : *node) {
                collectValuableArtifacts(child, results);
            }
        }
    }

    QString ExtractConfiguration::makeExtensionCompressionRule(const QString& ext) {
        return "\\." + ext + "$";
    }

    QString ExtractConfiguration::makeFileCompressionRule(const QString& fileName) {
        return "^" + fileName + "$";
    }

    QString ExtractConfiguration::getConfigFileName(const QDir& dir) {
        const QString configName("pbo");
        const QString configExt(".json");

        QString configFile = configName + configExt;
        if (!dir.exists(configFile)) {
            return dir.absoluteFilePath(configFile);
        }

        for (int i = 1; i < std::numeric_limits<int>::max(); i++) {
            configFile = configName + "-" + QString::number(i) + configExt;
            if (!dir.exists(configFile)) {
                return dir.absoluteFilePath(configFile);
            }
        }

        throw AppException("The code must never reach this line");
    }

}
