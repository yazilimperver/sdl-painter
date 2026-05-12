#include "sdl_painter/color.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/brush.h"

int main() {
    sdl_painter::Color red{255, 0, 0, 255};
    sdl_painter::Pen pen(red, 2.0f);
    sdl_painter::Brush brush(red);
    return 0;
}
