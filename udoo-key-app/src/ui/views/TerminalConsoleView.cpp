#include "TerminalConsoleView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>

namespace UdooKey {

TerminalConsoleView::TerminalConsoleView(TerminalClient* terminalClient, QWidget* parent)
    : QWidget(parent)
    , m_terminalClient(terminalClient) {
    setupUi();
    setTerminalClient(m_terminalClient);
}

void TerminalConsoleView::setTerminalClient(TerminalClient* client) {
    m_terminalClient = client;
    if (m_terminalClient) {
        connect(m_terminalClient, &TerminalClient::textReceived, this, &TerminalConsoleView::onTerminalTextReceived);
    }
}

void TerminalConsoleView::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Quick Command Bar
    auto quickBar = new QHBoxLayout();
    const QStringList quickCmds = {"help", "status", "adc", "cont", "i2c scan", "capture"};
    for (const auto& cmd : quickCmds) {
        auto btn = new QPushButton(cmd, this);
        btn->setProperty("cmd", cmd);
        connect(btn, &QPushButton::clicked, this, &TerminalConsoleView::onQuickCommandClicked);
        quickBar->addWidget(btn);
    }
    quickBar->addStretch();

    auto btnClear = new QPushButton("Clear Screen", this);
    connect(btnClear, &QPushButton::clicked, [this]() { m_txtTerminalOutput->clear(); });
    quickBar->addWidget(btnClear);

    layout->addLayout(quickBar);

    // Terminal Screen
    m_txtTerminalOutput = new QTextEdit(this);
    m_txtTerminalOutput->setReadOnly(true);
    m_txtTerminalOutput->setFont(QFont("Consolas", 10));
    m_txtTerminalOutput->setStyleSheet(
        "background-color: #101014; color: #33FF77; "
        "border: 1px solid #30303C; border-radius: 4px; padding: 6px;");
    layout->addWidget(m_txtTerminalOutput);

    // Command Line Input
    auto inputLayout = new QHBoxLayout();
    m_editCommandInput = new QLineEdit(this);
    m_editCommandInput->setPlaceholderText("Enter UDOO KEY terminal command (e.g. 'help', 'status', 'i2c scan')...");
    m_editCommandInput->setFont(QFont("Consolas", 10));
    m_editCommandInput->installEventFilter(this);

    m_btnSend = new QPushButton("Send", this);
    m_btnSend->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnSend, &QPushButton::clicked, this, &TerminalConsoleView::onSendCommandClicked);
    connect(m_editCommandInput, &QLineEdit::returnPressed, this, &TerminalConsoleView::onSendCommandClicked);

    inputLayout->addWidget(m_editCommandInput);
    inputLayout->addWidget(m_btnSend);
    layout->addLayout(inputLayout);
}

void TerminalConsoleView::onQuickCommandClicked() {
    auto btn = qobject_cast<QPushButton*>(sender());
    if (btn && m_terminalClient) {
        QString cmd = btn->property("cmd").toString();
        m_terminalClient->sendCommand(cmd);
    }
}

void TerminalConsoleView::onSendCommandClicked() {
    QString text = m_editCommandInput->text().trimmed();
    if (text.isEmpty() || !m_terminalClient) return;

    m_commandHistory.append(text);
    m_historyIndex = m_commandHistory.size();

    m_terminalClient->sendCommand(text);
    m_editCommandInput->clear();

    // Scroll to bottom
    m_txtTerminalOutput->verticalScrollBar()->setValue(m_txtTerminalOutput->verticalScrollBar()->maximum());
}

void TerminalConsoleView::onTerminalTextReceived(const QString& text) {
    QTextCursor cursor = m_txtTerminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);

    int i = 0;
    int len = text.length();
    QTextCharFormat fmt = cursor.charFormat();
    static const QColor defaultColor("#33FF77");

    while (i < len) {
        if (text[i] == QLatin1Char('\033')) {
            // Find end of ANSI escape sequence
            int escEnd = -1;
            for (int j = i + 1; j < std::min(len, i + 16); ++j) {
                QChar ch = text[j];
                if (ch.isLetter() || ch == QLatin1Char('~')) {
                    escEnd = j;
                    break;
                }
            }

            if (escEnd != -1) {
                QString seq = text.mid(i + 1, escEnd - i);
                if (seq == "[2J") {
                    m_txtTerminalOutput->clear();
                    cursor = m_txtTerminalOutput->textCursor();
                } else if (seq.startsWith("[") && seq.endsWith("m")) {
                    QString codesStr = seq.mid(1, seq.length() - 2);
                    QStringList codes = codesStr.split(';', Qt::SkipEmptyParts);
                    if (codes.isEmpty() || codes.contains("0")) {
                        fmt.setForeground(defaultColor);
                        fmt.setFontWeight(QFont::Normal);
                    }
                    if (codes.contains("1")) {
                        fmt.setFontWeight(QFont::Bold);
                    }
                    for (const QString& c : codes) {
                        if (c == "31" || c == "1;31") fmt.setForeground(QColor("#ff5252"));       // Bold/Standard Red
                        else if (c == "32" || c == "1;32") fmt.setForeground(QColor("#00e676"));  // Bold/Standard Green
                        else if (c == "33" || c == "1;33") fmt.setForeground(QColor("#ffd600"));  // Bold/Standard Yellow
                        else if (c == "34" || c == "1;34") fmt.setForeground(QColor("#448aff"));  // Bold/Standard Blue
                        else if (c == "35" || c == "1;35") fmt.setForeground(QColor("#e040fb"));  // Bold/Standard Magenta
                        else if (c == "36" || c == "1;36") fmt.setForeground(QColor("#00e5ff"));  // Bold/Standard Cyan
                        else if (c == "37" || c == "1;37") fmt.setForeground(QColor("#ffffff"));  // White
                        else if (c == "2;37") fmt.setForeground(QColor("#90a4ae"));               // Dim
                    }
                }
                i = escEnd + 1;
                continue;
            }
        }

        // Find next special control char (\033, \b, or \r)
        int nextSpecial = text.indexOf(QLatin1Char('\033'), i);
        int nextBs = text.indexOf(QLatin1Char('\b'), i);
        int chunkEnd = len;
        if (nextSpecial != -1 && nextSpecial < chunkEnd) chunkEnd = nextSpecial;
        if (nextBs != -1 && nextBs < chunkEnd) chunkEnd = nextBs;

        if (chunkEnd > i) {
            QString chunk = text.mid(i, chunkEnd - i);
            // Normalize raw \r\n to \n for QTextEdit block management
            chunk.remove(QLatin1Char('\r'));
            cursor.insertText(chunk, fmt);
            i = chunkEnd;
        } else if (i < len && text[i] == QLatin1Char('\b')) {
            // Handle destructive backspace
            cursor.deletePreviousChar();
            i++;
            if (i < len && text[i] == QLatin1Char(' ')) i++;
            if (i < len && text[i] == QLatin1Char('\b')) i++;
        } else {
            i++;
        }
    }

    m_txtTerminalOutput->setTextCursor(cursor);
    m_txtTerminalOutput->verticalScrollBar()->setValue(m_txtTerminalOutput->verticalScrollBar()->maximum());
}

bool TerminalConsoleView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_editCommandInput && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (!m_commandHistory.isEmpty() && m_historyIndex > 0) {
                m_historyIndex--;
                m_editCommandInput->setText(m_commandHistory[m_historyIndex]);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
            if (m_historyIndex + 1 < m_commandHistory.size()) {
                m_historyIndex++;
                m_editCommandInput->setText(m_commandHistory[m_historyIndex]);
            } else {
                m_historyIndex = m_commandHistory.size();
                m_editCommandInput->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace UdooKey
