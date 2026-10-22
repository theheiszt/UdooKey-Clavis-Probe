#include "HexViewWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QFontDatabase>

namespace UdooKey {

HexViewWidget::HexViewWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void HexViewWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    // Toolbar header
    auto headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(2, 2, 2, 2);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Find hex or text...");
    m_searchEdit->setMaximumWidth(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HexViewWidget::onSearchTextChanged);
    headerLayout->addWidget(m_searchEdit);

    m_jumpEdit = new QLineEdit(this);
    m_jumpEdit->setPlaceholderText("Offset (0x..)");
    m_jumpEdit->setMaximumWidth(100);
    headerLayout->addWidget(m_jumpEdit);

    auto btnJump = new QPushButton("Go", this);
    btnJump->setMaximumWidth(40);
    connect(btnJump, &QPushButton::clicked, this, &HexViewWidget::onJumpOffsetClicked);
    headerLayout->addWidget(btnJump);

    headerLayout->addStretch();

    auto btnCopyHex = new QPushButton("Copy Hex", this);
    btnCopyHex->setToolTip("Copy as space-separated hex bytes");
    connect(btnCopyHex, &QPushButton::clicked, this, &HexViewWidget::copyAsHex);
    headerLayout->addWidget(btnCopyHex);

    auto btnCopyC = new QPushButton("Copy C Array", this);
    btnCopyC->setToolTip("Copy as C/C++ byte array: 0x00, 0x01, ...");
    connect(btnCopyC, &QPushButton::clicked, this, &HexViewWidget::copyAsCArray);
    headerLayout->addWidget(btnCopyC);

    m_lblStats = new QLabel("0 bytes", this);
    m_lblStats->setStyleSheet("color: #8E8E9F; font-size: 11px;");
    headerLayout->addWidget(m_lblStats);

    mainLayout->addLayout(headerLayout);

    // Text view
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    
    // Choose fixed-pitch font
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    m_textEdit->setFont(font);

    m_textEdit->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #0E0E12;"
        "  color: #D4D4D4;"
        "  selection-background-color: #264F78;"
        "  selection-color: #FFFFFF;"
        "  border: 1px solid #2B2B36;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "}"
    );

    mainLayout->addWidget(m_textEdit);
}

void HexViewWidget::setData(const QByteArray& data) {
    m_data = data;
    renderHexView();
    m_lblStats->setText(QString("%1 bytes (%2 KB)")
                        .arg(m_data.size())
                        .arg(m_data.size() / 1024.0, 0, 'f', 2));
}

QByteArray HexViewWidget::data() const {
    return m_data;
}

void HexViewWidget::clear() {
    m_data.clear();
    m_textEdit->clear();
    m_lblStats->setText("0 bytes");
}

void HexViewWidget::renderHexView() {
    m_textEdit->clear();
    if (m_data.isEmpty()) {
        return;
    }

    QString formatted;
    // Preallocate rough capacity: each line has 16 bytes and is ~78 characters
    const int lines = (m_data.size() + 15) / 16;
    formatted.reserve(lines * 80);

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_data.constData());
    const int total = m_data.size();

    for (int offset = 0; offset < total; offset += 16) {
        // Offset column: 8 hex digits
        formatted.append(QString("%1  ").arg(offset, 8, 16, QChar('0')).toUpper());

        // Hex bytes (16 bytes, split into two 8-byte chunks)
        for (int i = 0; i < 16; ++i) {
            if (i == 8) {
                formatted.append(" "); // extra middle gap
            }
            if (offset + i < total) {
                formatted.append(QString("%1 ").arg(raw[offset + i], 2, 16, QChar('0')).toUpper());
            } else {
                formatted.append("   ");
            }
        }

        formatted.append(" |");

        // ASCII column
        for (int i = 0; i < 16; ++i) {
            if (offset + i < total) {
                uint8_t ch = raw[offset + i];
                if (ch >= 32 && ch <= 126) {
                    formatted.append(QChar(ch));
                } else {
                    formatted.append('.');
                }
            } else {
                formatted.append(' ');
            }
        }

        formatted.append("|\n");
    }

    m_textEdit->setPlainText(formatted);
}

void HexViewWidget::scrollToOffset(int offset) {
    if (offset < 0 || offset >= m_data.size()) return;
    int line = offset / 16;
    QTextCursor cursor(m_textEdit->document()->findBlockByLineNumber(line));
    m_textEdit->setTextCursor(cursor);
    m_textEdit->centerCursor();
}

void HexViewWidget::highlightRange(int offset, int length) {
    if (offset < 0 || offset >= m_data.size()) return;
    scrollToOffset(offset);
    // Find text cursor and select
    // In our format, each line is 78 chars (excluding newline)
    // Offset line:
    int line = offset / 16;
    int byteInLine = offset % 16;
    int charPosInHex = 10 + (byteInLine * 3) + (byteInLine >= 8 ? 1 : 0);

    QTextBlock block = m_textEdit->document()->findBlockByLineNumber(line);
    if (!block.isValid()) return;

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, charPosInHex);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, std::min(length * 3, 48));
    m_textEdit->setTextCursor(cursor);
}

void HexViewWidget::onJumpOffsetClicked() {
    QString txt = m_jumpEdit->text().trimmed();
    bool ok = false;
    int offset = 0;
    if (txt.startsWith("0x", Qt::CaseInsensitive)) {
        offset = txt.mid(2).toInt(&ok, 16);
    } else {
        offset = txt.toInt(&ok, 10);
    }

    if (ok) {
        scrollToOffset(offset);
    }
}

void HexViewWidget::onSearchTextChanged(const QString& text) {
    if (text.isEmpty()) return;
    search(text);
}

void HexViewWidget::search(const QString& query) {
    if (query.isEmpty() || m_data.isEmpty()) return;

    // Search in raw data either as hex or ASCII
    QByteArray needle;
    QString clean = query;
    clean.remove(' ');
    bool isHex = false;
    if (clean.length() % 2 == 0) {
        needle = QByteArray::fromHex(clean.toUtf8());
        if (!needle.isEmpty()) isHex = true;
    }

    int foundIdx = -1;
    if (isHex) {
        foundIdx = m_data.indexOf(needle);
    }
    if (foundIdx < 0) {
        // Fallback to ASCII search
        foundIdx = m_data.indexOf(query.toUtf8());
    }

    if (foundIdx >= 0) {
        scrollToOffset(foundIdx);
        highlightRange(foundIdx, isHex ? needle.size() : query.size());
    }
}

void HexViewWidget::copyAsHex() {
    if (m_data.isEmpty()) return;
    QString hexStr;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_data.constData());
    for (int i = 0; i < m_data.size(); ++i) {
        hexStr.append(QString("%1 ").arg(raw[i], 2, 16, QChar('0')).toUpper());
    }
    QApplication::clipboard()->setText(hexStr.trimmed());
}

void HexViewWidget::copyAsAscii() {
    if (m_data.isEmpty()) return;
    QApplication::clipboard()->setText(QString::fromUtf8(m_data));
}

void HexViewWidget::copyAsCArray() {
    if (m_data.isEmpty()) return;
    QString cCode = "const uint8_t payload[] = {\n    ";
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_data.constData());
    for (int i = 0; i < m_data.size(); ++i) {
        cCode.append(QString("0x%1").arg(raw[i], 2, 16, QChar('0')).toUpper());
        if (i < m_data.size() - 1) {
            cCode.append(", ");
        }
        if ((i + 1) % 12 == 0 && i < m_data.size() - 1) {
            cCode.append("\n    ");
        }
    }
    cCode.append("\n};\n");
    QApplication::clipboard()->setText(cCode);
}

} // namespace UdooKey
