#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief Temel vertex — pozisyon + renk.
struct Vertex {
  float x{0.0f};
  float y{0.0f};
  uint8_t r{255}, g{255}, b{255}, a{255};

  constexpr Vertex() = default;
  constexpr Vertex(float x_, float y_) : x(x_), y(y_) {}
  constexpr Vertex(float x_, float y_, uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_)
      : x(x_), y(y_), r(r_), g(g_), b(b_), a(a_) {}
};

/// @brief Doku koordinatlı vertex — pozisyon + UV + renk (tint).
struct TexturedVertex {
  float x{0.0f};
  float y{0.0f};
  float u{0.0f};
  float v{0.0f};
  uint8_t r{255}, g{255}, b{255}, a{255};

  constexpr TexturedVertex() = default;
  constexpr TexturedVertex(float x_, float y_, float u_, float v_)
      : x(x_), y(y_), u(u_), v(v_) {}
  constexpr TexturedVertex(float x_, float y_, float u_, float v_,
                           uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_)
      : x(x_), y(y_), u(u_), v(v_), r(r_), g(g_), b(b_), a(a_) {}
};

}  // namespace sdl_painter
