#include "sdl_painter/path.h"

#include <algorithm>
#include <cmath>

namespace sdl_painter {

namespace {

/// @brief Vektörün uzunluğu.
float Length(float dx, float dy) {
  return std::sqrt((dx * dx) + (dy * dy));
}

/// @brief Bézier ikinci fark vektörünün uzunluğu: |a - 2b + c|.
///
/// Hem quadratic hem cubic eğrinin ikinci türev sınırı bu büyüklükten
/// türetilir (bkz. @ref Path::SegmentsForCurve).
float SecondDifference(const Point& a, const Point& b, const Point& c) {
  return Length(a.x - (2.0F * b.x) + c.x, a.y - (2.0F * b.y) + c.y);
}

}  // namespace

Path::Path(float flatness)
    : mFlatness(flatness > 0.0F ? flatness : kDefaultFlatness) {}

void Path::MoveTo(float x, float y) {
  DropDegenerateSubPath();
  mCurrent = Point(x, y);
  mSubPathStart = mCurrent;
  mPendingStart = true;
}

void Path::MoveTo(const Point& point) {
  MoveTo(point.x, point.y);
}

void Path::LineTo(float x, float y) {
  // Hic baslangic yoksa ilk cagri cizgi degil, konumlandirmadir.
  if (mSubPaths.empty() && !mPendingStart) {
    MoveTo(x, y);
    return;
  }
  AppendPoint(Point(x, y));
}

void Path::LineTo(const Point& point) {
  LineTo(point.x, point.y);
}

void Path::QuadTo(float cx, float cy, float x, float y) {
  EnsureStarted();

  const Point p0 = mCurrent;
  const Point p1(cx, cy);
  const Point p2(x, y);

  // B''(t) = 2 (P0 - 2 P1 + P2) — t'den bagimsiz, yani sinir tam deger.
  const int32_t segments =
      SegmentsForCurve(2.0F * SecondDifference(p0, p1, p2));

  for (int32_t i = 1; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float u = 1.0F - t;
    const float w0 = u * u;
    const float w1 = 2.0F * u * t;
    const float w2 = t * t;
    AppendPoint(Point((w0 * p0.x) + (w1 * p1.x) + (w2 * p2.x),
                      (w0 * p0.y) + (w1 * p1.y) + (w2 * p2.y)));
  }
}

void Path::QuadTo(const Point& control, const Point& end) {
  QuadTo(control.x, control.y, end.x, end.y);
}

void Path::CubicTo(float c1x, float c1y, float c2x, float c2y, float x,
                   float y) {
  EnsureStarted();

  const Point p0 = mCurrent;
  const Point p1(c1x, c1y);
  const Point p2(c2x, c2y);
  const Point p3(x, y);

  // B''(t) = 6 [(1-t)(P0 - 2 P1 + P2) + t (P1 - 2 P2 + P3)] — iki ucun
  // buyugu tum aralik icin ust sinirdir (dogrusal interpolasyon).
  const float bound = 6.0F * std::max(SecondDifference(p0, p1, p2),
                                      SecondDifference(p1, p2, p3));
  const int32_t segments = SegmentsForCurve(bound);

  for (int32_t i = 1; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float u = 1.0F - t;
    const float w0 = u * u * u;
    const float w1 = 3.0F * u * u * t;
    const float w2 = 3.0F * u * t * t;
    const float w3 = t * t * t;
    AppendPoint(Point((w0 * p0.x) + (w1 * p1.x) + (w2 * p2.x) + (w3 * p3.x),
                      (w0 * p0.y) + (w1 * p1.y) + (w2 * p2.y) + (w3 * p3.y)));
  }
}

void Path::CubicTo(const Point& control1, const Point& control2,
                   const Point& end) {
  CubicTo(control1.x, control1.y, control2.x, control2.y, end.x, end.y);
}

void Path::Close() {
  // Bekleyen bir baslangic varsa acik bir parca yoktur — kapatilacak sey yok.
  if (!mPendingStart && !mSubPaths.empty() &&
      mSubPaths.back().points.size() >= 2) {
    mSubPaths.back().closed = true;
  }
  mCurrent = mSubPathStart;
  mPendingStart = true;
}

void Path::Clear() noexcept {
  mSubPaths.clear();
  mCurrent = Point();
  mSubPathStart = Point();
  mPendingStart = false;
}

bool Path::IsEmpty() const noexcept {
  return std::none_of(
      mSubPaths.begin(), mSubPaths.end(),
      [](const SubPath& sub) { return sub.points.size() >= 2; });
}

std::size_t Path::PointCount() const noexcept {
  std::size_t total = 0;
  for (const SubPath& sub : mSubPaths) {
    total += sub.points.size();
  }
  return total;
}

void Path::DropDegenerateSubPath() {
  if (!mSubPaths.empty() && mSubPaths.back().points.size() < 2) {
    mSubPaths.pop_back();
  }
}

void Path::EnsureStarted() {
  if (!mPendingStart && !mSubPaths.empty()) {
    return;
  }
  mSubPaths.emplace_back();
  mSubPaths.back().points.push_back(mCurrent);
  mPendingStart = false;
}

void Path::AppendPoint(const Point& point) {
  EnsureStarted();
  mSubPaths.back().points.push_back(point);
  mCurrent = point;
}

int32_t Path::SegmentsForCurve(float second_derivative_bound) const {
  // Duz (veya dejenere) egri: tek dogru parcasi yeter.
  if (!(second_derivative_bound > 0.0F)) {  // NaN korumasi da bu testte
    return 1;
  }
  const float needed = std::sqrt(second_derivative_bound / (8.0F * mFlatness));
  if (!(needed > 1.0F)) {
    return 1;
  }
  if (needed >= static_cast<float>(kMaxCurveSegments)) {
    return kMaxCurveSegments;
  }
  return static_cast<int32_t>(std::ceil(needed));
}

}  // namespace sdl_painter
