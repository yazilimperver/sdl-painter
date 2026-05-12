#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief 2D nokta — float koordinatlar.
struct Point {
  float x{0.0f};
  float y{0.0f};

  constexpr Point() = default;
  constexpr Point(float x, float y) : x(x), y(y) {}

  [[nodiscard]] Point operator+(const Point& other) const noexcept {
    return {x + other.x, y + other.y};
  }
  [[nodiscard]] Point operator-(const Point& other) const noexcept {
    return {x - other.x, y - other.y};
  }
  [[nodiscard]] Point operator*(float s) const noexcept {
    return {x * s, y * s};
  }

  [[nodiscard]] bool operator==(const Point& other) const noexcept {
    return x == other.x && y == other.y;
  }
  [[nodiscard]] bool operator!=(const Point& other) const noexcept {
    return !(*this == other);
  }
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
  [[nodiscard]] float Right() const noexcept { return x + w; }

  /// @brief Dikdörtgenin alt kenarının y koordinatı.
  [[nodiscard]] float Bottom() const noexcept { return y + h; }

  /// @brief Merkez noktası.
  [[nodiscard]] Point Center() const noexcept {
    return {x + w * 0.5f, y + h * 0.5f};
  }

  /// @brief Verilen nokta dikdörtgen içinde mi?
  [[nodiscard]] bool Contains(float px, float py) const noexcept {
    return px >= x && px <= Right() && py >= y && py <= Bottom();
  }

  [[nodiscard]] bool operator==(const Rect& other) const noexcept {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }
  [[nodiscard]] bool operator!=(const Rect& other) const noexcept {
    return !(*this == other);
  }
};

/// @brief 2D boyut — genişlik ve yükseklik.
struct Size {
  float w{0.0f};
  float h{0.0f};

  constexpr Size() = default;
  constexpr Size(float w, float h) : w(w), h(h) {}

  [[nodiscard]] bool operator==(const Size& other) const noexcept {
    return w == other.w && h == other.h;
  }
  [[nodiscard]] bool operator!=(const Size& other) const noexcept {
    return !(*this == other);
  }
};

}  // namespace sdl_painter
