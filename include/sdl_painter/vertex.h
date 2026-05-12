#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief Temel vertex — pozisyon + renk.
struct Vertex {
  float x{0.0f};
  float y{0.0f};
  uint8_t r{255}, g{255}, b{255}, a{255};

  constexpr Vertex() = default;
  constexpr Vertex(float x, float y) : x(x), y(y) {}
  constexpr Vertex(float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
      : x(x), y(y), r(r), g(g), b(b), a(a) {}
};

/// @brief Doku koordinatlı vertex — pozisyon + UV + renk (tint).
struct TexturedVertex {
  float x{0.0f};
  float y{0.0f};
  float u{0.0f};
  float v{0.0f};
  uint8_t r{255}, g{255}, b{255}, a{255};

  constexpr TexturedVertex() = default;
  constexpr TexturedVertex(float x, float y, float u, float v)
      : x(x), y(y), u(u), v(v) {}
  constexpr TexturedVertex(float x, float y, float u, float v,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a)
      : x(x), y(y), u(u), v(v), r(r), g(g), b(b), a(a) {}
};

}  // namespace sdl_painter
