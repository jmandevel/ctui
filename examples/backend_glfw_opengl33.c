#include "frontend_common.h"
#include <ctui/ctui.h>

int main() {
  CTUI_Context *ctx = CTUI_createContext();
  if (ctx == NULL) {
    return 1;
  }
  const char *cp437_16x16_images[] = {"cp437_16x16.png"};
  CTUI_Font *font_16x16 =
      CTUI_createFont("cp437_16x16.ctuifont", cp437_16x16_images, 1);
  if (font_16x16 == NULL) {
    return 2;
  }
  const char *cp437_8x16_images[] = {"cp437_8x16.png"};
  CTUI_Font *font_8x16 =
      CTUI_createFont("cp437_8x16.ctuifont", cp437_8x16_images, 1);
  if (font_8x16 == NULL) {
    return 2;
  }
  const CTUI_DVector2 tile_pixel_wh = {16, 16};
  const size_t layer_count = 2;
  const CTUI_LayerInfo infos[2] = {
      (CTUI_LayerInfo){.font = font_16x16, .tile_div_wh = {1, 1}},
      (CTUI_LayerInfo){.font = font_8x16, .tile_div_wh = {2, 1}}};
  const CTUI_ColorMode color = CTUIC_RGBA32;
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