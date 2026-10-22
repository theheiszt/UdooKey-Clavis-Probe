#pragma once

#include <QString>
#include <functional>
#include <memory>
#include "CaptureBuffer.h"
#include "TaskManager.h"

namespace UdooKey {

class IExporter {
public:
    virtual ~IExporter() = default;

    virtual QString name() const = 0;
    virtual QString defaultExtension() const = 0;
    virtual QString fileFilter() const = 0;

    virtual bool exportData(const QString& filePath, const CaptureBuffer& buffer, QString* errorMsg = nullptr) {
        return exportDataAsync(filePath, buffer, errorMsg, nullptr, nullptr);
    }

    virtual bool exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                                QString* errorMsg = nullptr,
                                std::shared_ptr<CancellationToken> token = nullptr,
                                std::function<void(int, const QString&)> progress = nullptr) = 0;
};

} // namespace UdooKey
