#include "CsvExporter.h"
#include <QFile>
#include <QTextStream>

namespace UdooKey {

bool CsvExporter::exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                                  QString* errorMsg,
                                  std::shared_ptr<CancellationToken> token,
                                  std::function<void(int, const QString&)> progress) {
    if (buffer.isEmpty()) {
        if (errorMsg) *errorMsg = "Capture buffer is empty";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }

    QTextStream out(&file);

    // Header
    out << "Sample,Time_s,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7\n";

    uint32_t count = buffer.sampleCount();
    uint32_t rate = buffer.sampleRateHz();
    double timePerSample = (rate > 0) ? (1.0 / rate) : 1e-7;
    uint32_t progressInterval = std::max<uint32_t>(1000, count / 50);

    for (uint32_t i = 0; i < count; ++i) {
        if (token && token->isCancelled()) {
            file.close();
            file.remove();
            if (errorMsg) *errorMsg = "Export operation cancelled by user";
            return false;
        }

        if (progress && (i % progressInterval == 0)) {
            int pct = static_cast<int>((static_cast<uint64_t>(i) * 100) / count);
            progress(pct, QString("Writing CSV: %1% (%2/%3 samples)").arg(pct).arg(i).arg(count));
        }

        uint8_t byte = buffer.sampleAt(i);
        double timeSec = i * timePerSample;
        out << i << "," << QString::number(timeSec, 'g', 10);
        for (int ch = 0; ch < 8; ++ch) {
            int val = (byte & (1 << ch)) ? 1 : 0;
            out << "," << val;
        }
        out << "\n";
    }

    if (progress) {
        progress(100, "CSV export completed");
    }

    file.close();
    return true;
}

} // namespace UdooKey
