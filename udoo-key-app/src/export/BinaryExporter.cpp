#include "BinaryExporter.h"
#include <QFile>

namespace UdooKey {

bool BinaryExporter::exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
                                     QString* errorMsg,
                                     std::shared_ptr<CancellationToken> token,
                                     std::function<void(int, const QString&)> progress) {
    if (buffer.isEmpty()) {
        if (errorMsg) *errorMsg = "Capture buffer is empty";
        return false;
    }

    if (token && token->isCancelled()) {
        if (errorMsg) *errorMsg = "Export operation cancelled by user";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }

    if (progress) progress(10, "Writing binary payload...");

    qint64 total = buffer.sampleCount();
    const char* data = buffer.rawData();
    qint64 written = 0;
    const qint64 chunkSize = 65536;

    while (written < total) {
        if (token && token->isCancelled()) {
            file.close();
            file.remove();
            if (errorMsg) *errorMsg = "Export operation cancelled by user";
            return false;
        }

        qint64 toWrite = std::min(chunkSize, total - written);
        qint64 w = file.write(data + written, toWrite);
        if (w <= 0) {
            file.close();
            if (errorMsg) *errorMsg = "Failed to write complete binary payload";
            return false;
        }
        written += w;

        if (progress) {
            int pct = static_cast<int>((written * 100) / total);
            progress(pct, QString("Writing BIN: %1%").arg(pct));
        }
    }

    file.close();
    if (progress) progress(100, "Binary export completed");
    return true;
}

} // namespace UdooKey
