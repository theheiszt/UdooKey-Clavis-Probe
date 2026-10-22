#include "VcdExporter.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

namespace UdooKey {

bool VcdExporter::exportDataAsync(const QString& filePath, const CaptureBuffer& buffer,
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

    // VCD Header
    out << "$date\n  " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n$end\n";
    out << "$version\n  UDOO KEY Companion App v1.0.0\n$end\n";
    
    // Timescale: 1ns or 10ns or 100ns based on sample rate
    uint32_t rate = buffer.sampleRateHz();
    double sampleTimeNs = (1.0e9 / (rate > 0 ? rate : 10'000'000));
    out << "$timescale 1ns $end\n";
    out << "$scope module udoo_key $end\n";

    // Define 8 wire signals: CH0..CH7 with symbols !..$ (ASCII 33..40)
    for (int ch = 0; ch < 8; ++ch) {
        char symbol = static_cast<char>('!' + ch);
        out << "$var wire 1 " << symbol << " CH" << ch << " $end\n";
    }
    out << "$upscope $end\n";
    out << "$enddefinitions $end\n";
    out << "$dumpvars\n";

    // Initial values at sample 0
    uint8_t prevByte = buffer.sampleAt(0);
    for (int ch = 0; ch < 8; ++ch) {
        char symbol = static_cast<char>('!' + ch);
        int val = (prevByte & (1 << ch)) ? 1 : 0;
        out << val << symbol << "\n";
    }
    out << "$end\n";

    // Dump value changes
    uint32_t count = buffer.sampleCount();
    uint32_t progressInterval = std::max<uint32_t>(1000, count / 50);

    for (uint32_t i = 1; i < count; ++i) {
        if (token && token->isCancelled()) {
            file.close();
            file.remove();
            if (errorMsg) *errorMsg = "Export operation cancelled by user";
            return false;
        }

        if (progress && (i % progressInterval == 0)) {
            int pct = static_cast<int>((static_cast<uint64_t>(i) * 100) / count);
            progress(pct, QString("Writing VCD: %1% (%2/%3 samples)").arg(pct).arg(i).arg(count));
        }

        uint8_t curByte = buffer.sampleAt(i);
        if (curByte != prevByte) {
            uint64_t timestampNs = static_cast<uint64_t>(i * sampleTimeNs);
            out << "#" << timestampNs << "\n";
            for (int ch = 0; ch < 8; ++ch) {
                int oldVal = (prevByte & (1 << ch)) ? 1 : 0;
                int newVal = (curByte & (1 << ch)) ? 1 : 0;
                if (oldVal != newVal) {
                    char symbol = static_cast<char>('!' + ch);
                    out << newVal << symbol << "\n";
                }
            }
            prevByte = curByte;
        }
    }

    if (progress) {
        progress(100, "VCD export completed");
    }

    file.close();
    return true;
}

} // namespace UdooKey
