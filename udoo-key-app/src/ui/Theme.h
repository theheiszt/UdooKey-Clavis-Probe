#pragma once

#include <QString>
#include <QColor>

namespace UdooKey {

class Theme {
public:
    static QString darkStyleSheet();

    // 8-Channel default high-contrast distinct colors
    static QColor channelColor(uint8_t channel);
    static QString channelColorHex(uint8_t channel);

    static constexpr const char* COLOR_BG_DARK = "#1E1E24";
    static constexpr const char* COLOR_PANEL_BG = "#26262F";
    static constexpr const char* COLOR_BORDER = "#3A3A46";
    static constexpr const char* COLOR_ACCENT = "#00D2A0"; // UDOO Cyan-Teal
    static constexpr const char* COLOR_TEXT = "#E0E0E6";
    static constexpr const char* COLOR_TEXT_MUTED = "#8E8E9F";
    static constexpr const char* COLOR_CURSOR_A = "#FF5555";
    static constexpr const char* COLOR_CURSOR_B = "#55AAFF";
};

} // namespace UdooKey
