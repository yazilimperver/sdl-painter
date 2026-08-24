# Example assets

Files the demos load at runtime. They are copied next to the example
executables at build time, so the demos find them through `SDL_GetBasePath()`
rather than a hard-coded source-tree path.

> The **library** ships no assets. Shaders are embedded in the binary
> ([ADR-009](../../adr/ADR-009-embedded-shaders.md)) precisely so that a
> consumer never has to copy files around. What lives here belongs to the
> examples only, and none of it is installed.

## `rpg_character_walk.png`

| | |
|---|---|
| **Licence** | [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) (public domain) |
| **Author** | arikel |
| **Source** | <https://opengameart.org/content/2d-rpg-character-walk-spritesheet> |
| **Used by** | [`graphics/sprite_animation.cpp`](../graphics/sprite_animation.cpp) |

192×128 RGBA, an **8 × 4** grid of 24×32 frames:

| Row | Direction |
|---|---|
| 0 | facing the viewer (down) |
| 1 | facing away (up) |
| 2 | facing left |
| 3 | facing right |

Rows 2 and 3 are exact horizontal mirrors of each other — verified frame by
frame. The `sprite_animation` demo uses that fact: it draws the right-facing
row either from the sheet or by flipping the left-facing row, and the two are
pixel-identical. That is the argument for `ImageFlip` in one picture — a sheet
only needs to carry one side.

The upstream page offers the work under CC0 **and** CC-BY 4.0; CC0 is the one
taken here, so no attribution is legally required. It is given anyway.
