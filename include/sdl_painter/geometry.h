#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief 2D nokta — float koordinatlar.
struct Point {
  float x{0.0f};
  float y{0.0f};

  constexpr Point() = default;
  constexpr Point(float x, float y) : x(x), y(y) {}

  Point operator+(const Point& other) const { return {x + other.x, y + other.y}; }
  Point operator-(const Point& other) const { return {x - other.x, y - other.y}; }
  Point operator*(float s) const { return {x * s, y * s}; }

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }
  bool operator!=(const Point& other) const { return !(*this == other); }
};

/// @brief 2D dikdörtgen — sol üst köşe + genişlik/yükseklik.
struct Rect {
  float x{0.0f};
  float y{0.0f};
  float w{0.0f};
  float h{0.0f};

  constexpr Rect() = default;
  constexpr Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}

  /// @brief Dikdörtgenin sağ kenarının x koordinatı.
  float Right() const { return x + w; }

  /// @brief Dikdörtgenin alt kenarının y koordinatı.
  float Bottom() const { return y + h; }

  /// @brief Merkez noktası.
  Point Center() const { return {x + w * 0.5f, y + h * 0.5f}; }

  /// @brief Verilen nokta dikdörtgen içinde mi?
  bool Contains(float px, float py) const {
    return px >= x && px <= Right() && py >= y && py <= Bottom();
  }

  bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }
  bool operator!=(const Rect& other) const { return !(*this == other); }
};

/// @brief 2D boyut — genişlik ve yükseklik.
struct Size {
  float w{0.0f};
  float h{0.0f};

  constexpr Size() = default;
  constexpr Size(float w, float h) : w(w), h(h) {}

  bool operator==(const Size& other) const {
    return w == other.w && h == other.h;
  }
  bool operator!=(const Size& other) const { return !(*this == other); }
};

}  // namespace sdl_painter
