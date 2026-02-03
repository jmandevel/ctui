#include <ctui/ctui.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <fnv/fnv.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

int CTUI_getHasRealTerminal() {
#ifdef _WIN32
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD modeIn, modeOut;
  return GetConsoleMode(hIn, &modeIn) && GetConsoleMode(hOut, &modeOut);
#else
  return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

static void CTUI_initEventQueue(CTUI_Context *ctx) {
  ctx->_event_queue_capacity = 32;
  ctx->_event_queue = calloc(ctx->_event_queue_capacity, sizeof(CTUI_Event));
  ctx->_event_queue_count = 0;
  ctx->_event_queue_head = 0;
}

static void CTUI_freeEventQueue(CTUI_Context *ctx) {
  if (ctx->_event_queue != NULL) {
    free(ctx->_event_queue);
  }
  ctx->_event_queue = NULL;
  ctx->_event_queue_capacity = 0;
  ctx->_event_queue_count = 0;
  ctx->_event_queue_head = 0;
}

int CTUI_nextEvent(CTUI_Context *ctx, CTUI_Event *event) {
  if (ctx->_event_queue_count > 0) {
    size_t idx = ctx->_event_queue_head;
    *event = ctx->_event_queue[idx];
    ctx->_event_queue_head =
        (ctx->_event_queue_head + 1) % ctx->_event_queue_capacity;
    ctx->_event_queue_count--;
    return 1;
  }
  return 0;
}

void CTUI_clear(CTUI_Console *console) {
  console->_fill_bg_set = 0;
  for (size_t l = 0; l < console->_layer_count; ++l) {
    CTUI_ConsoleLayer *layer = &console->_layers[l];
    layer->_tiles_count = 0;
  }
}

int CTUI_hasConsole(CTUI_Context *context) {
  return context->_first_console != NULL;
}

static int CTUI_ensureCapacity(CTUI_ConsoleLayer *layer, size_t capacity) {
  if (layer->_tiles_capacity < capacity) {
    size_t new_capacity =
        layer->_tiles_capacity == 0 ? 64 : layer->_tiles_capacity * 2;
    while (new_capacity < capacity) {
      new_capacity *= 2;
    }
    CTUI_ConsoleTile *new_tiles =
        realloc(layer->_tiles, sizeof(CTUI_ConsoleTile) * new_capacity);
    if (new_tiles == NULL) {
      // TODO
      return 0;
    }
    layer->_tiles = new_tiles;
    layer->_tiles_capacity = new_capacity;
  }
  return 1;
}

static CTUI_ConsoleTile *CTUI_getNewTile(CTUI_ConsoleLayer *layer) {
  if (!CTUI_ensureCapacity(layer, layer->_tiles_count + 1)) {
    return NULL;
  }
  return &layer->_tiles[layer->_tiles_count++];
}

void CTUI_pushCodepoint(CTUI_ConsoleLayer *layer, uint32_t codepoint,
                        CTUI_IVector2 pos_xy, CTUI_Color fg, CTUI_Color bg) {
  if (pos_xy.x < 0 || pos_xy.y < 0) {
    return;
  }
  CTUI_ConsoleTile *tile = CTUI_getNewTile(layer);
  if (tile == NULL) {
    // TODO
    return;
  }
  tile->_pos_xy = (CTUI_SVector2){.x = (size_t)pos_xy.x, .y = (size_t)pos_xy.y};
  tile->_codepoint = codepoint;
  tile->_fg = fg;
  tile->_bg = bg;
}

void CTUI_fill(CTUI_Console *console, CTUI_Color bg) {
  console->_fill_bg_color = bg;
  console->_fill_bg_set = 1;
}

void CTUI_refresh(struct CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->refresh != NULL) {
    console->_platform->refresh(console);
  }
}

CTUI_Context *CTUI_createContext() {
  CTUI_Context *ctx = (CTUI_Context *)calloc(1, sizeof(CTUI_Context));
  CTUI_initEventQueue(ctx);
  return ctx;
}

CTUI_Font *CTUI_createFont(const char *ctuifont_path, const char **image_paths,
                           size_t image_count) {
  if (image_paths == NULL || image_count == 0) {
    return NULL;
  }

  FILE *fp = fopen(ctuifont_path, "r");
  if (fp == NULL) {
    // TODO
    return NULL;
  }

  CTUI_Font *font = calloc(1, sizeof(CTUI_Font));
  if (font == NULL) {
    // TODO
    fclose(fp);
    return NULL;
  }

  // Read font name line.
  char font_name[256];
  if (fgets(font_name, sizeof(font_name), fp) == NULL) {
    // TODO
    free(font);
    fclose(fp);
    return NULL;
  }

  // Read tile dimensions and blend mode.
  int tile_w, tile_h;
  char blend_mode[64];
  if (fscanf(fp, "%d %d %63s", &tile_w, &tile_h, blend_mode) != 3) {
    // TODO
    free(font);
    fclose(fp);
    return NULL;
  }

  // Load first image to get dimensions.
  int img_w, img_h, img_channels;
  unsigned char *first_pixels =
      stbi_load(image_paths[0], &img_w, &img_h, &img_channels, 4);
  if (first_pixels == NULL) {
    // TODO
    free(font);
    fclose(fp);
    return NULL;
  }

  // Allocate combined pixel buffer for all pages.
  size_t page_size = (size_t)img_w * (size_t)img_h * 4;
  unsigned char *all_pixels = malloc(page_size * image_count);
  if (all_pixels == NULL) {
    stbi_image_free(first_pixels);
    free(font);
    fclose(fp);
    return NULL;
  }

  // Copy first page.
  memcpy(all_pixels, first_pixels, page_size);
  stbi_image_free(first_pixels);

  // Load remaining pages.
  for (size_t i = 1; i < image_count; i++) {
    int page_w, page_h, page_channels;
    unsigned char *page_pixels =
        stbi_load(image_paths[i], &page_w, &page_h, &page_channels, 4);
    if (page_pixels == NULL || page_w != img_w || page_h != img_h) {
      // TODO: dimensions must match
      if (page_pixels != NULL)
        stbi_image_free(page_pixels);
      free(all_pixels);
      free(font);
      fclose(fp);
      return NULL;
    }
    memcpy(all_pixels + i * page_size, page_pixels, page_size);
    stbi_image_free(page_pixels);
  }

  font->_image._width = img_w;
  font->_image._height = img_h;
  font->_image._pages = image_count;
  font->_image._pixels = all_pixels;

  // Count glyphs first.
  size_t glyph_count = 0;
  long glyph_start_pos = ftell(fp);
  int left, right, top, bottom, page;
  uint32_t codepoint;
  while (fscanf(fp, "%d %d %d %d %d %u", &left, &right, &top, &bottom, &page,
                &codepoint) == 6) {
    // Skip rest of line. (comments)
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n')
      ;
    glyph_count++;
  }

  // Allocate glyph map.
  font->_map_size = (size_t)(glyph_count * 1.5f) + 1;
  font->_glyph_map = calloc(font->_map_size, sizeof(CTUI_Glyph));
  if (font->_glyph_map == NULL) {
    // TODO
    free(all_pixels);
    free(font);
    fclose(fp);
    return NULL;
  }

  // Rewind and read glyphs.
  fseek(fp, glyph_start_pos, SEEK_SET);
  font->_max_map_offset = 0;

  while (fscanf(fp, "%d %d %d %d %d %u", &left, &right, &top, &bottom, &page,
                &codepoint) == 6) {
    // Skip rest of line. (comments)
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n')
      ;

    CTUI_Glyph glyph = {0};
    glyph._codepoint = codepoint;
    glyph._tiles_wh.x = 1;
    glyph._tiles_wh.y = 1;
    glyph._tex_coords.s = (float)left / (float)img_w;
    glyph._tex_coords.t = (float)right / (float)img_w;
    glyph._tex_coords.p = (float)top / (float)img_h;
    glyph._tex_coords.q = (float)bottom / (float)img_h;
    glyph._tex_coords.page = (float)page;

    // Insert into hash map.
    const uint64_t hash = FNV_hashBuffer64_1a(&codepoint, sizeof(uint32_t));
    const size_t initial_slot_i = hash % font->_map_size;
    size_t offset;
    for (offset = 0; offset < font->_map_size; offset++) {
      size_t slot_i = (initial_slot_i + offset) % font->_map_size;
      if (font->_glyph_map[slot_i]._tiles_wh.x == 0) {
        font->_glyph_map[slot_i] = glyph;
        break;
      }
    }
    if (offset > font->_max_map_offset) {
      font->_max_map_offset = offset;
    }
  }

  fclose(fp);
  return font;
}

void CTUI_destroyFont(CTUI_Font *font) {
  if (font->_image._pixels != NULL) {
    free(font->_image._pixels);
  }
  if (font->_glyph_map != NULL) {
    free(font->_glyph_map);
  }
  free(font);
}

CTUI_Glyph *CTUI_tryGetGlyph(CTUI_Font *font, uint32_t codepoint) {
  const uint64_t hash = FNV_hashBuffer64_1a(&codepoint, sizeof(uint32_t));
  const size_t initial_slot_i = hash % font->_map_size;
  for (size_t offset = 0; offset <= font->_max_map_offset; offset++) {
    size_t slot_i = (initial_slot_i + offset) % font->_map_size;
    if (font->_glyph_map[slot_i]._codepoint == codepoint &&
        font->_glyph_map[slot_i]._tiles_wh.x != 0) {
      return &font->_glyph_map[slot_i];
    }
  }
  return NULL;
}

void CTUI_pollEvents(CTUI_Context *ctx) {
  CTUI_Console *console = ctx->_first_console;
  while (console != NULL) {
    if (console->_platform != NULL && console->_platform->pollEvents != NULL) {
      console->_platform->pollEvents(console);
    }
    console = console->_next;
  }
}

void CTUI_pushEvent(CTUI_Context *ctx, CTUI_Event *event) {
  if (ctx->_event_queue_count == ctx->_event_queue_capacity) {
    size_t new_capacity = ctx->_event_queue_capacity * 2;
    CTUI_Event *new_queue =
        realloc(ctx->_event_queue, sizeof(CTUI_Event) * new_capacity);
    if (new_queue == NULL) {
      // TODO
      return;
    }
    ctx->_event_queue = new_queue;
    ctx->_event_queue_capacity = new_capacity;
  }
  size_t tail_i = (ctx->_event_queue_head + ctx->_event_queue_count) %
                  ctx->_event_queue_capacity;
  ctx->_event_queue[tail_i] = *event;
  ctx->_event_queue_count++;
}

void CTUI_destroyConsole(CTUI_Console *console) {
  if (console->_ctx != NULL) {
    if (console->_ctx->_first_console == console) {
      console->_ctx->_first_console = console->_next;
    }
    if (console->_prev != NULL) {
      console->_prev->_next = console->_next;
    }
    if (console->_next != NULL) {
      console->_next->_prev = console->_prev;
    }
  }
  if (console->_platform != NULL && console->_platform->destroy != NULL) {
    console->_platform->destroy(console);
  }
}

void CTUI_destroyContext(CTUI_Context *ctx) {
  CTUI_Console *console = ctx->_first_console;
  while (console != NULL) {
    CTUI_Console *next = console->_next;
    CTUI_destroyConsole(console);
    console = next;
  }
  CTUI_freeEventQueue(ctx);
  free(ctx);
}

int CTUI_getIsWindow(CTUI_Console *console) {
  return !console->_is_real_terminal;
}

int CTUI_getIsFullscreen(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getIsFullscreen != NULL) {
    return console->_platform->getIsFullscreen(console);
  }
  return 0;
}

int CTUI_getHasViewport(CTUI_Console *console) {
  return !console->_is_real_terminal;
}

CTUI_DVector2 CTUI_getCursorViewportPos(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getCursorViewportPos != NULL) {
    return console->_platform->getCursorViewportPos(console);
  }
  CTUI_DVector2 result = {0, 0};
  return result;
}

CTUI_DVector2 CTUI_getCursorTilePos(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getCursorTilePos != NULL) {
    return console->_platform->getCursorTilePos(console);
  }
  CTUI_DVector2 result = {0, 0};
  return result;
}

int CTUI_getMouseButton(CTUI_Console *console, CTUI_MouseButton button) {
  if (console->_platform != NULL &&
      console->_platform->getMouseButton != NULL) {
    return console->_platform->getMouseButton(console, button);
  }
  return 0;
}

int CTUI_getKeyState(CTUI_Console *console, CTUI_Key key) {
  if (console->_platform != NULL && console->_platform->getKeyState != NULL) {
    return console->_platform->getKeyState(console, key);
  }
  return 0;
}

void CTUI_transformViewport(CTUI_Console *console, CTUI_FVector2 translation,
                            CTUI_FVector2 scale) {
  if (console->_platform != NULL &&
      console->_platform->transformViewport != NULL) {
    console->_platform->transformViewport(console, translation, scale);
  }
}

void CTUI_resetViewport(CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->resetViewport != NULL) {
    console->_platform->resetViewport(console);
  }
}

void CTUI_setWindowPixelWh(CTUI_Console *console, CTUI_IVector2 pixel_wh) {
  if (console->_platform != NULL &&
      console->_platform->setWindowPixelWh != NULL) {
    console->_platform->setWindowPixelWh(console, pixel_wh);
  }
}

CTUI_IVector2 CTUI_getWindowPixelWh(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowPixelWh != NULL) {
    return console->_platform->getWindowPixelWh(console);
  }
  CTUI_IVector2 result = {0, 0};
  return result;
}

void CTUI_setViewportTileWh(CTUI_Console *console, CTUI_SVector2 tile_wh) {
  if (console->_platform != NULL &&
      console->_platform->setViewportTileWh != NULL) {
    console->_platform->setViewportTileWh(console, tile_wh);
  }
}

void CTUI_fitWindowPixelWhToViewportTileWh(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->fitWindowPixelWhToViewportTileWh != NULL) {
    console->_platform->fitWindowPixelWhToViewportTileWh(console);
  }
}

void CTUI_fitViewportTileWhToWindowPixelWh(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->fitViewportTileWhToWindowPixelWh != NULL) {
    console->_platform->fitViewportTileWhToWindowPixelWh(console);
  }
}

void CTUI_setWindowResizable(CTUI_Console *console, int resizable) {
  if (console->_platform != NULL &&
      console->_platform->setWindowResizable != NULL) {
    console->_platform->setWindowResizable(console, resizable);
  }
}

int CTUI_getWindowResizable(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowResizable != NULL) {
    return console->_platform->getWindowResizable(console);
  }
  return 0;
}

void CTUI_setWindowDecorated(CTUI_Console *console, int decorated) {
  if (console->_platform != NULL &&
      console->_platform->setWindowDecorated != NULL) {
    console->_platform->setWindowDecorated(console, decorated);
  }
}

int CTUI_getWindowDecorated(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowDecorated != NULL) {
    return console->_platform->getWindowDecorated(console);
  }
  return 1; // default decorated
}

void CTUI_setWindowFloating(CTUI_Console *console, int floating) {
  if (console->_platform != NULL &&
      console->_platform->setWindowFloating != NULL) {
    console->_platform->setWindowFloating(console, floating);
  }
}

int CTUI_getWindowFloating(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowFloating != NULL) {
    return console->_platform->getWindowFloating(console);
  }
  return 0;
}

void CTUI_minimizeWindow(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->minimizeWindow != NULL) {
    console->_platform->minimizeWindow(console);
  }
}

void CTUI_maximizeWindow(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->maximizeWindow != NULL) {
    console->_platform->maximizeWindow(console);
  }
}

void CTUI_restoreWindow(CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->restoreWindow != NULL) {
    console->_platform->restoreWindow(console);
  }
}

int CTUI_getWindowMinimized(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowMinimized != NULL) {
    return console->_platform->getWindowMinimized(console);
  }
  return 0;
}

int CTUI_getWindowMaximized(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowMaximized != NULL) {
    return console->_platform->getWindowMaximized(console);
  }
  return 0;
}

void CTUI_focusWindow(CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->focusWindow != NULL) {
    console->_platform->focusWindow(console);
  }
}

int CTUI_getWindowFocused(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowFocused != NULL) {
    return console->_platform->getWindowFocused(console);
  }
  return 0;
}

void CTUI_requestWindowAttention(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->requestWindowAttention != NULL) {
    console->_platform->requestWindowAttention(console);
  }
}

void CTUI_setWindowOpacity(CTUI_Console *console, float opacity) {
  if (console->_platform != NULL &&
      console->_platform->setWindowOpacity != NULL) {
    console->_platform->setWindowOpacity(console, opacity);
  }
}

float CTUI_getWindowOpacity(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->getWindowOpacity != NULL) {
    return console->_platform->getWindowOpacity(console);
  }
  return 1.0f;
}

CTUI_SVector2 CTUI_getGlyphTilesWh(const CTUI_Glyph *glyph) {
  return glyph->_tiles_wh;
}

uint32_t CTUI_getGlyphCodepoint(const CTUI_Glyph *glyph) {
  return glyph->_codepoint;
}

CTUI_Stpqp CTUI_getGlyphTexCoords(const CTUI_Glyph *glyph) {
  return glyph->_tex_coords;
}

size_t CTUI_getFontImageWidth(const CTUI_Font *font) {
  return font->_image._width;
}

size_t CTUI_getFontImageHeight(const CTUI_Font *font) {
  return font->_image._height;
}

size_t CTUI_getFontImagePages(const CTUI_Font *font) {
  return font->_image._pages;
}

CTUI_DVector2 CTUI_getLayerTileDivWh(const CTUI_ConsoleLayer *layer) {
  return layer->_tile_div_wh;
}

void CTUI_setLayerTileDivWh(CTUI_Console *console, size_t layer_i,
                            CTUI_DVector2 tile_div_wh) {
  if (layer_i >= console->_layer_count) {
    return;
  }
  // Ensure non-zero divisors
  if (tile_div_wh.x == 0)
    tile_div_wh.x = 1;
  if (tile_div_wh.y == 0)
    tile_div_wh.y = 1;
  console->_layers[layer_i]._tile_div_wh = tile_div_wh;
  // Clear layer data
  console->_layers[layer_i]._tiles_count = 0;
}

const CTUI_Font *CTUI_getLayerFont(const CTUI_ConsoleLayer *layer) {
  return layer->_font;
}

void CTUI_setLayerFont(CTUI_Console *console, size_t layer_i, CTUI_Font *font) {
  if (layer_i >= console->_layer_count) {
    return;
  }
  console->_layers[layer_i]._font = font;
  // Clear layer data
  console->_layers[layer_i]._tiles_count = 0;
}

size_t CTUI_getLayerTilesCount(const CTUI_ConsoleLayer *layer) {
  return layer->_tiles_count;
}

CTUI_SVector2 CTUI_getConsoleTileWh(const CTUI_Console *console) {
  return console->_console_tile_wh;
}

size_t CTUI_getConsoleLayerCount(const CTUI_Console *console) {
  return console->_layer_count;
}

CTUI_ConsoleLayer *CTUI_getConsoleLayer(CTUI_Console *console, size_t layer_i) {
  if (layer_i >= console->_layer_count) {
    return NULL;
  }
  return &console->_layers[layer_i];
}

CTUI_ColorMode CTUI_getConsoleColorMode(const CTUI_Console *console) {
  return console->_effective_color_mode;
}

int CTUI_getConsoleIsRealTerminal(const CTUI_Console *console) {
  return console->_is_real_terminal;
}
