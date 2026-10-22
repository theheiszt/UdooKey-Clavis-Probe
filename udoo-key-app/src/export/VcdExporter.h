#pragma once

#include "IExporter.h"

namespace UdooKey {

class VcdExporter : public IExporter {
public:
    VcdExporter() = default;
    ~VcdExporter() override = default;

    QString name() const override { return "Value Change Dump (VCD)"; }
    QString defaultExtension() const override { return "vcd"; }
    QString fileFilter() const override { return "VCD Files (*.vcd);;All Files (*.*)"; }

    bool exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                         QString* errorMsg = nullptr,
                         std::shared_ptr<CancellationToken> token = nullptr,
                         std::function<void(int, const QString&)> progress = nullptr) override;
};

} // namespace UdooKey
