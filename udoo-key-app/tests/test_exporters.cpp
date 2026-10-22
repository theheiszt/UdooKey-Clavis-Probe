#include "VcdExporter.h"
#include "CsvExporter.h"
#include "BinaryExporter.h"
#include "CaptureBuffer.h"
#include <QFile>
#include <iostream>

bool runExporterTests() {
    using namespace UdooKey;

    QByteArray testData;
    for (int i = 0; i < 256; ++i) {
        testData.append(static_cast<char>(i & 0xFF));
    }
    CaptureBuffer buf(testData, 1000000);

    // 1. Test VCD Exporter
    QString vcdPath = "test_export.vcd";
    VcdExporter vcdExp;
    QString err;
    if (!vcdExp.exportData(vcdPath, buf, &err)) return false;

    QFile vcdFile(vcdPath);
    if (!vcdFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString vcdContent = vcdFile.readAll();
    vcdFile.close();
    vcdFile.remove(); // Clean up

    if (!vcdContent.contains("$version")) return false;
    if (!vcdContent.contains("$var wire 1 ! CH0 $end")) return false;
    if (!vcdContent.contains("$dumpvars")) return false;

    // 2. Test CSV Exporter
    QString csvPath = "test_export.csv";
    CsvExporter csvExp;
    if (!csvExp.exportData(csvPath, buf, &err)) return false;

    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString csvContent = csvFile.readAll();
    csvFile.close();
    csvFile.remove(); // Clean up

    if (!csvContent.startsWith("Sample,Time_s,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7")) return false;

    // 3. Test Binary Exporter
    QString binPath = "test_export.bin";
    BinaryExporter binExp;
    if (!binExp.exportData(binPath, buf, &err)) return false;

    QFile binFile(binPath);
    if (!binFile.open(QIODevice::ReadOnly)) return false;
    QByteArray binContent = binFile.readAll();
    binFile.close();
    binFile.remove(); // Clean up

    if (binContent.size() != 256) return false;
    if (binContent != testData) return false;

    return true;
}
