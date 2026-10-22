#pragma once

#include "IExporter.h"

namespace UdooKey {

class BinaryExporter : public IExporter {
public:
    BinaryExporter() = default;
    ~BinaryExporter() override = default;

    QString name() const override { return "Raw Packed Binary (*.bin)"; }
    QString defaultExtension() const override { return "bin"; }
    QString fileFilter() const override { return "Binary Files (*.bin);;All Files (*.*)"; }

    bool exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                         QString* errorMsg = nullptr,
                         std::shared_ptr<CancellationToken> token = nullptr,
                         std::function<void(int, const QString&)> progress = nullptr) override;
};

} // namespace UdooKey
