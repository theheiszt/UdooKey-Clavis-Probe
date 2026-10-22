#pragma once

#include <QWidget>
#include <QByteArray>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace UdooKey {

class HexViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit HexViewWidget(QWidget* parent = nullptr);
    ~HexViewWidget() override = default;

    void setData(const QByteArray& data);
    QByteArray data() const;
    void clear();

    void scrollToOffset(int offset);
    void highlightRange(int offset, int length);

signals:
    void offsetClicked(int offset);

public slots:
    void copyAsHex();
    void copyAsAscii();
    void copyAsCArray();
    void search(const QString& query);

private slots:
    void onSearchTextChanged(const QString& text);
    void onJumpOffsetClicked();

private:
    void setupUi();
    void renderHexView();

    QByteArray m_data;
    QPlainTextEdit* m_textEdit;
    QLineEdit* m_searchEdit;
    QLineEdit* m_jumpEdit;
    QLabel* m_lblStats;
};

} // namespace UdooKey
