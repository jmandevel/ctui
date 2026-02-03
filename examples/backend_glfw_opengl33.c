#include "frontend_common.h"
#include <ctui/ctui.h>

int main() {
  CTUI_Context *ctx = CTUI_createContext();
  if (ctx == NULL) {
    return 1;
  }
  const char *font_images[] = {"spartan_16x16_cp437.png"};
  CTUI_Font *font =
      CTUI_createFont("spartan_16x16_cp437.ctuifont", font_images, 1);
  if (font == NULL) {
    return 2;
  }
  const CTUI_DVector2 tile_pixel_wh = {16, 16};
  const size_t layer_count = 2;
  const CTUI_LayerInfo infos[2] = {
      (CTUI_LayerInfo){.font = font, .tile_div_wh = {1, 1}},
      (CTUI_LayerInfo){.font = font, .tile_div_wh = {1, 1}}};
  const CTUI_ColorMode color = CTUI_COLORMODE_FULL;
  const char *title = "glfw opengl33 window";
  CTUI_Console *console = CTUI_createGlfwOpengl33FakeTerminal(
      ctx, tile_pixel_wh, layer_count, infos, color, title);
  if (console == NULL) {
    return 3;
  }
  CTUI_setWindowedTileWh(console, (CTUI_SVector2){80, 60});
  CTUI_setWindowResizable(console, 1);
  CTUI_showWindow(console);
  if (!startApp(ctx, console)) {
    return 4;
  }
  while (CTUI_hasConsole(ctx)) {
    if (!runMainLoopBody(ctx, console)) {
      break;
    }
  }
  cleanup(ctx, console);
  CTUI_destroyConsole(console);
  CTUI_destroyContext(ctx);
  return 0;
}