#pragma once

#include <QColor>
#include <QIcon>

// Line icons painted with QPainter on a 24-unit grid. Painting them here keeps one
// visual style across the toolbar and panels and avoids depending on the SVG icon
// engine plugin being deployed next to the executable.
namespace icons {

enum class Glyph {
    Scan,
    Link,
    Unlink,
    LinkAll,
    Stop,
    Settings,
    Fit,
    Target,
    Obstacle,
    Trash,
    Play,
    Pause,
    Check,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Single,
    Fleet,
    Flash,
    Log,
    Keyboard,
    Home,
    Route,
};

inline constexpr char kInk[] = "#405660";
inline constexpr char kAccent[] = "#087f72";
inline constexpr char kDanger[] = "#d74950";

// Returns an icon with normal and disabled states rendered for high-DPI screens.
QIcon make(Glyph glyph, const QColor &color = QColor(kInk));

} // namespace icons
