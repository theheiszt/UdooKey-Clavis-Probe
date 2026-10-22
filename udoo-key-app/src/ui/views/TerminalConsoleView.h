#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include "TerminalClient.h"

namespace UdooKey {

class TerminalConsoleView : public QWidget {
    Q_OBJECT

public:
    explicit TerminalConsoleView(TerminalClient* terminalClient, QWidget* parent = nullptr);
    ~TerminalConsoleView() override = default;

    void setTerminalClient(TerminalClient* client);

private slots:
    void onSendCommandClicked();
    void onTerminalTextReceived(const QString& text);
    void onQuickCommandClicked();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();

    TerminalClient* m_terminalClient;
    QTextEdit* m_txtTerminalOutput;
    QLineEdit* m_editCommandInput;
    QPushButton* m_btnSend;

    QStringList m_commandHistory;
    int m_historyIndex = -1;
};

} // namespace UdooKey
