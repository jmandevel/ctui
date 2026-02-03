#include <ctui/ctui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMILEY_CODEPOINT 9786

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  CTUI_Context *ctx = CTUI_createContext();
  CTUI_Console *ncurses_console = NULL;
  CTUI_Console *glfw_console = NULL;
  CTUI_Font *font = NULL;

  // COLORS
  CTUI_Color fg = {
      .palette8 = 3,
      .palette16 = 11,
      .palette256 = 226,
      .full = {255, 255, 0, 255} // Yellow
  };
  CTUI_Color bg = {
      .palette8 = 0, .palette16 = 0, .palette256 = 0, .full = {0, 0, 0, 255}
      // Black
  };
  CTUI_Color fill_bg = {
      .palette8 = 4, .palette16 = 4, .palette256 = 17, .full = {0, 0, 64, 255}
      // Dark blue
  };

  // Smiley positions for each console.
  int ncurses_smiley_x = 10;
  int ncurses_smiley_y = 5;
  int glfw_smiley_x = 20;
  int glfw_smiley_y = 10;

  // Zoom and pan state for GLFW window.
  float zoom = 1.0f;
  float pan_x = 0.0f;
  float pan_y = 0.0f;
  int is_panning = 0;
  double last_mouse_x = 0.0;
  double last_mouse_y = 0.0;

  int glfw_w = 40;
  int glfw_h = 20;
  int ncurses_w = 40;
  int ncurses_h = 20;

  // Create ncurses terminal first.
  if (CTUI_getHasRealTerminal()) {
    ncurses_console =
        CTUI_createNcursesRealTerminal(ctx, 1, CTUI_COLORMODE_256);
    if (ncurses_console) {
      CTUI_SVector2 tile_wh = CTUI_getConsoleTileWh(ncurses_console);
      ncurses_w = tile_wh.x;
      ncurses_h = tile_wh.y;
      ncurses_smiley_x = ncurses_w / 2;
      ncurses_smiley_y = ncurses_h / 2;
    }
  }

  // Load font and create GLFW window.
  const char *font_images[] = {"spartan_16x16_cp437.png"};
  font = CTUI_createFont("spartan_16x16_cp437.ctuifont", font_images, 1);
  if (font) {
    CTUI_DVector2 tile_pixel_wh = {16.0, 16.0};
    // Changing tile_div_wh alows to set how many times you divide tile_pixel_wh
    // for a layer Example: setting it to {2, 1} means that the tiles of this
    // layer are 1/2 a tile_pixel_wh.x wide.
    CTUI_LayerInfo layer_infos[] = {{.font = font, .tile_div_wh = {1.0, 1.0}}};
    // NOTE: glfw terminals are hidden by default. need to set size after creating it for it to be visible!
    // (can set windowed or fullscreen)
    glfw_console = CTUI_createGlfwOpengl33FakeTerminal(
        ctx, tile_pixel_wh, 1, layer_infos, CTUI_COLORMODE_FULL,
        "CTUI GLFW - Arrow Keys to Move");
    if (glfw_console) {
      CTUI_SVector2 window_tiles = {glfw_w, glfw_h};
      // Size the window based on tile dimensions, then show it.
      CTUI_setWindowedTileWh(glfw_console, window_tiles);
    }
  } else {
    return 1;
  }

  if (!ncurses_console && !glfw_console) {
    if (font)
      CTUI_destroyFont(font);
    CTUI_destroyContext(ctx);
    return 1;
  }

  // Main Loop
  while (CTUI_hasConsole(ctx)) {
    CTUI_pollEvents(ctx); // call this before event loop to gather all events...

    // Event Loop
    CTUI_Event event;
    while (CTUI_nextEvent(ctx, &event)) {
      if (event.type == CTUI_EVENT_KEY &&
          (event.data.key.action == CTUIA_PRESS)) {
        int *smiley_x = NULL;
        int *smiley_y = NULL;
        int max_w = 0;
        int max_h = 0;

        if (event.console == ncurses_console) {
          smiley_x = &ncurses_smiley_x;
          smiley_y = &ncurses_smiley_y;
          max_w = ncurses_w;
          max_h = ncurses_h;
        } else if (event.console == glfw_console) {
          smiley_x = &glfw_smiley_x;
          smiley_y = &glfw_smiley_y;
          max_w = glfw_w;
          max_h = glfw_h;
        }

        if (smiley_x && smiley_y) {
          switch (event.data.key.key) {
          case CTUIK_UP:
            if (*smiley_y > 0)
              (*smiley_y)--;
            break;
          case CTUIK_DOWN:
            if (*smiley_y < max_h - 1)
              (*smiley_y)++;
            break;
          case CTUIK_LEFT:
            if (*smiley_x > 0)
              (*smiley_x)--;
            break;
          case CTUIK_RIGHT:
            if (*smiley_x < max_w - 1)
              (*smiley_x)++;
            break;
          case CTUIK_ESCAPE:
            if (font) {
              CTUI_destroyFont(font);
            }
            CTUI_destroyContext(ctx);
            return 0;
          case CTUIK_F:
            // Toggle fullscreen.
            if (event.console == glfw_console) {
              if (CTUI_getIsFullscreen(glfw_console)) {
                // Go windowed.
                CTUI_SVector2 window_tiles = {40, 20};
                CTUI_setWindowedTileWh(glfw_console, window_tiles);
                glfw_w = window_tiles.x;
                glfw_h = window_tiles.y;
              } else {
                // Go fullscreen.
                CTUI_setWindowedFullscreen(glfw_console);
                CTUI_SVector2 tile_wh = CTUI_getConsoleTileWh(glfw_console);
                glfw_w = tile_wh.x;
                glfw_h = tile_wh.y;
              }
            }
            break;
          case CTUIK_R:
            // Reset transform, fit tiles to window, reset smiley position.
            if (event.console == glfw_console) {
              zoom = 1.0f;
              pan_x = 0.0f;
              pan_y = 0.0f;
              CTUI_resetViewport(glfw_console);
              CTUI_fitViewportTileWhToWindowPixelWh(glfw_console);
              CTUI_SVector2 tile_wh = CTUI_getConsoleTileWh(glfw_console);
              glfw_w = tile_wh.x;
              glfw_h = tile_wh.y;
              glfw_smiley_x = 0;
              glfw_smiley_y = 0;
            }
            break;
          default:
            break;
          }
        }
      } else if (event.type == CTUI_EVENT_CLOSE) {
        // Window close button pressed.
        CTUI_destroyConsole(event.console);
        if (event.console == glfw_console) {
          glfw_console = NULL;
        }
        if (event.console == ncurses_console) {
          ncurses_console = NULL;
        }
      } else if (event.type == CTUI_EVENT_RESIZE) {
        if (event.console == ncurses_console) {
          ncurses_w = event.data.resize.console_tile_wh.x;
          ncurses_h = event.data.resize.console_tile_wh.y;
          if (ncurses_smiley_x >= ncurses_w)
            ncurses_smiley_x = ncurses_w - 1;
          if (ncurses_smiley_y >= ncurses_h)
            ncurses_smiley_y = ncurses_h - 1;
        }
      } else if (event.type == CTUI_EVENT_SCROLL &&
                 event.console == glfw_console) {
        // Zoom with scroll wheel.
        float zoom_factor = 1.1f;
        if (event.data.scroll.scroll_xy.y > 0) {
          zoom *= zoom_factor;
        } else if (event.data.scroll.scroll_xy.y < 0) {
          zoom /= zoom_factor;
        }
        // Clamp zoom.
        if (zoom < 0.1f)
          zoom = 0.1f;
        if (zoom > 10.0f)
          zoom = 10.0f;
      } else if (event.type == CTUI_EVENT_MOUSE_BUTTON &&
                 event.console == glfw_console) {
        // Start/stop panning with right click.
        if (event.data.mouse_button.button == CTUIMB_RIGHT) {
          if (event.data.mouse_button.action == CTUIA_PRESS) {
            is_panning = 1;
            CTUI_DVector2 cursor = CTUI_getCursorViewportPos(glfw_console);
            last_mouse_x = cursor.x;
            last_mouse_y = cursor.y;
          } else {
            is_panning = 0;
          }
        }
      } else if (event.type == CTUI_EVENT_CURSOR_POS &&
                 event.console == glfw_console) {
        // Pan while right mouse button is held.
        if (is_panning) {
          CTUI_DVector2 cursor = CTUI_getCursorViewportPos(glfw_console);
          double dx = cursor.x - last_mouse_x;
          double dy = cursor.y - last_mouse_y;
          // Convert pixel delta to normalized coordinates.
          pan_x += (float)(dx * 2.0 / (glfw_w * 16.0));
          pan_y -= (float)(dy * 2.0 / (glfw_h * 16.0));
          last_mouse_x = cursor.x;
          last_mouse_y = cursor.y;
        }
      }
    }

    // Apply zoom/pan transform to GLFW console.
    if (glfw_console) {
      CTUI_FVector2 translation = {pan_x, pan_y};
      CTUI_FVector2 scale = {zoom, zoom};
      CTUI_transformViewport(glfw_console, translation, scale);
    }

    // Draw ncurses console.
    if (ncurses_console) {
      CTUI_clear(ncurses_console);
      CTUI_fill(ncurses_console, fill_bg);
      CTUI_IVector2 pos = {ncurses_smiley_x, ncurses_smiley_y};
      CTUI_pushCodepoint(CTUI_getConsoleLayer(ncurses_console, 0),
                         SMILEY_CODEPOINT, pos, fg, bg);
      CTUI_refresh(ncurses_console);
    }

    // Draw GLFW console.
    if (glfw_console) {
      CTUI_clear(glfw_console);
      CTUI_fill(glfw_console, fill_bg);
      CTUI_IVector2 pos = {glfw_smiley_x, glfw_smiley_y};
      CTUI_pushCodepoint(CTUI_getConsoleLayer(glfw_console, 0),
                         SMILEY_CODEPOINT, pos, fg, bg);
      CTUI_refresh(glfw_console);
    }
  }
  if (font) {
    CTUI_destroyFont(font);
  }
  CTUI_destroyContext(ctx);
  return 0;
}
