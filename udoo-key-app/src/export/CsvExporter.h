#pragma once

#include "IExporter.h"

namespace UdooKey {

class CsvExporter : public IExporter {
public:
    CsvExporter() = default;
    ~CsvExporter() override = default;

    QString name() const override { return "Comma-Separated Values (CSV)"; }
    QString defaultExtension() const override { return "csv"; }
    QString fileFilter() const override { return "CSV Files (*.csv);;All Files (*.*)"; }

    bool exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                         QString* errorMsg = nullptr,
                         std::shared_ptr<CancellationToken> token = nullptr,
                         std::function<void(int, const QString&)> progress = nullptr) override;
};

} // namespace UdooKey
