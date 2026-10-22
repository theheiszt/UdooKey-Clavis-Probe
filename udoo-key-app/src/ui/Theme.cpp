#include "Theme.h"

namespace UdooKey {

static const QColor s_channelColors[8] = {
    QColor(0, 220, 255),   // CH0: Bright Cyan
    QColor(255, 180, 0),   // CH1: Amber Orange
    QColor(50, 255, 120),  // CH2: Bright Green
    QColor(255, 60, 120),  // CH3: Magenta Pink
    QColor(180, 100, 255), // CH4: Lavender Purple
    QColor(255, 230, 40),  // CH5: Electric Yellow
    QColor(0, 160, 255),   // CH6: Sky Blue
    QColor(255, 120, 50)   // CH7: Coral Orange
};

QColor Theme::channelColor(uint8_t channel) {
    if (channel < 8) return s_channelColors[channel];
    return QColor(200, 200, 200);
}

QString Theme::channelColorHex(uint8_t channel) {
    return channelColor(channel).name();
}

QString Theme::darkStyleSheet() {
    return R"(
        QMainWindow, QDialog {
            background-color: #1A1A20;
            color: #E2E2E8;
            font-family: 'Segoe UI', 'Ubuntu', 'Roboto', sans-serif;
            font-size: 13px;
        }
        QWidget {
            background-color: #1A1A20;
            color: #E2E2E8;
            selection-background-color: #00D2A0;
            selection-color: #121216;
        }
        QTabBar::tab {
            background-color: #24242C;
            color: #A0A0B0;
            padding: 8px 18px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background-color: #32323E;
            color: #00D2A0;
            border-bottom: 2px solid #00D2A0;
        }
        QTabBar::tab:hover:!selected {
            background-color: #2B2B35;
            color: #E0E0E8;
        }
        QTabWidget::pane {
            border: 1px solid #33333E;
            background-color: #1E1E24;
            top: -1px;
        }
        QPushButton {
            background-color: #2A2A34;
            border: 1px solid #404050;
            border-radius: 4px;
            color: #ECECF0;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #383846;
            border-color: #00D2A0;
            color: #FFFFFF;
        }
        QPushButton:pressed {
            background-color: #00D2A0;
            color: #121216;
        }
        QPushButton:disabled {
            background-color: #202026;
            border-color: #30303A;
            color: #60606E;
        }
        QComboBox, QSpinBox, QLineEdit {
            background-color: #22222A;
            border: 1px solid #3D3D4C;
            border-radius: 4px;
            padding: 5px 8px;
            color: #ECECF0;
        }
        QComboBox:focus, QSpinBox:focus, QLineEdit:focus {
            border: 1px solid #00D2A0;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QGroupBox {
            border: 1px solid #363644;
            border-radius: 5px;
            margin-top: 12px;
            font-weight: bold;
            color: #00D2A0;
            padding-top: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
        }
        QTableWidget, QTreeWidget, QListWidget {
            background-color: #1E1E26;
            border: 1px solid #333340;
            gridline-color: #2E2E3A;
            color: #E0E0E6;
        }
        QHeaderView::section {
            background-color: #262632;
            color: #A0A0B0;
            padding: 5px;
            border: 1px solid #333340;
            font-weight: bold;
        }
        QScrollBar:vertical {
            background: #18181E;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3A3A4A;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #00D2A0;
        }
        QScrollBar:horizontal {
            background: #18181E;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #3A3A4A;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #00D2A0;
        }
        QStatusBar {
            background-color: #16161C;
            color: #9090A0;
            border-top: 1px solid #282832;
        }
        QToolTip {
            background-color: #282834;
            color: #FFFFFF;
            border: 1px solid #00D2A0;
            padding: 4px;
        }
    )";
}

} // namespace UdooKey
