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
#include <time.h>
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

int CTUI_rendererInit(CTUI_Renderer *r) {
  return r->vtable->init ? r->vtable->init(r) : 0;
}

void CTUI_rendererDestroy(CTUI_Renderer *r) {
  if (r->vtable->destroy)
    r->vtable->destroy(r);
}

void CTUI_rendererResize(CTUI_Renderer *r, int w, int h) {
  if (r->vtable->resize)
    r->vtable->resize(r, w, h);
}

void CTUI_rendererRender(CTUI_Renderer *r, CTUI_Console *c) {
  if (r->vtable->render)
    r->vtable->render(r, c);
}

void CTUI_rendererMakeCurrent(CTUI_Renderer *r) {
  if (r->vtable->makeCurrent)
    r->vtable->makeCurrent(r);
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

int CTUI_hasConsole(CTUI_Context *context) {
  return context->_first_console != NULL;
}

void CTUI_pushCodepoint(CTUI_ConsoleLayer *layer, uint32_t codepoint,
                        CTUI_IVector2 pos_xy, CTUI_Color fg, CTUI_Color bg) {
  if (pos_xy.x < 0 || pos_xy.y < 0) {
    return;
  }
  CTUI_Console *console = layer->_console;
  if (console->_platform != NULL && console->_platform->pushCodepoint != NULL) {
    console->_platform->pushCodepoint(layer, codepoint, pos_xy, fg, bg);
  }
}

uint32_t CTUI_decodeUtf8Cstr(const char **str) {
  const unsigned char *s = (const unsigned char *)*str;
  if (*s == 0) {
    return 0;
  }

  uint32_t codepoint;
  int bytes;

  if ((s[0] & 0x80) == 0) {
    // 1-byte ASCII: 0xxxxxxx
    codepoint = s[0];
    bytes = 1;
  } else if ((s[0] & 0xE0) == 0xC0) {
    // 2-byte: 110xxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80) {
      *str += 1;
      return UTF32_REPLACEMENT_CHARACTER;
    }
    codepoint = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    if (codepoint < 0x80) {
      *str += 2;
      return UTF32_REPLACEMENT_CHARACTER;
    }
    bytes = 2;
  } else if ((s[0] & 0xF0) == 0xE0) {
    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
      *str += 1;
      return UTF32_REPLACEMENT_CHARACTER;
    }
    codepoint = ((uint32_t)(s[0] & 0x0F) << 12) |
                ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    if (codepoint < 0x800) {
      *str += 3;
      return UTF32_REPLACEMENT_CHARACTER;
    }
    bytes = 3;
  } else if ((s[0] & 0xF8) == 0xF0) {
    // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
        (s[3] & 0xC0) != 0x80) {
      *str += 1;
      return 0xFFFD;
    }
    codepoint = ((uint32_t)(s[0] & 0x07) << 18) |
                ((uint32_t)(s[1] & 0x3F) << 12) |
                ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
      *str += 4;
      return UTF32_REPLACEMENT_CHARACTER;
    }
    bytes = 4;
  } else {
    // invalid leading byte
    *str += 1;
    return UTF32_REPLACEMENT_CHARACTER;
  }

  *str += bytes;
  return codepoint;
}

void CTUI_pushCstr(CTUI_ConsoleLayer *layer, const char *text,
                   CTUI_IVector2 pos_xy, size_t wrap_width, size_t max_height,
                   CTUI_Color fg, CTUI_Color bg) {
  if (text == NULL) {
    return;
  }
  int x = pos_xy.x;
  int y = pos_xy.y;
  int start_x = pos_xy.x;
  size_t col = 0;
  size_t line = 0;
  while (*text != '\0') {
    if (max_height > 0 && line >= max_height) {
      break;
    }
    uint32_t codepoint = CTUI_decodeUtf8Cstr(&text);
    if (codepoint == 0) {
      break;
    }
    if (codepoint == '\n') {
      x = start_x;
      y++;
      col = 0;
      line++;
      continue;
    }
    if (codepoint == '\r') {
      continue;
    }
    if (wrap_width > 0 && col >= wrap_width) {
      x = start_x;
      y++;
      col = 0;
      line++;
      if (max_height > 0 && line >= max_height) {
        break;
      }
    }
    CTUI_pushCodepoint(layer, codepoint, (CTUI_IVector2){x, y}, fg, bg);
    x++;
    col++;
  }
  return;
}

void CTUI_fill(CTUI_ConsoleLayer *layer, uint32_t codepoint, CTUI_Color fg,
               CTUI_Color bg) {
  if (layer->_console->_platform->fill) {
    layer->_console->_platform->fill(layer, codepoint, fg, bg);
  }
}

void CTUI_refresh(CTUI_Context* ctx) {
  if (ctx->_target_frame_ns > 0) {
    uint64_t target_frame_ns = ctx->_target_frame_ns;
    uint64_t last_frame_ns = ctx->_last_frame_ns;
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    if (last_frame_ns > 0) {
      uint64_t elapsed_ns = now_ns - last_frame_ns;
      if (elapsed_ns < target_frame_ns) {
        uint64_t sleep_ns = target_frame_ns - elapsed_ns;
        struct timespec sleep_time = {
          .tv_sec = (time_t)(sleep_ns / 1000000000ULL),
          .tv_nsec = (long)(sleep_ns % 1000000000ULL)
        };
        nanosleep(&sleep_time, NULL);
      }
    }
    timespec_get(&ts, TIME_UTC);
    ctx->_last_frame_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  }
  for (CTUI_Console* console = ctx->_first_console; console != NULL; console = console->_next)
  {
    if (console->_platform != NULL && console->_platform->refresh != NULL) {
      console->_platform->refresh(console);
    }
  }
}

CTUI_Context *CTUI_createContext() {
  CTUI_Context *ctx = (CTUI_Context *)calloc(1, sizeof(CTUI_Context));
  CTUI_initEventQueue(ctx);
  ctx->_target_frame_ns = CTUI_NS_FOR_FPS(60);
  ctx->_last_frame_ns = 0;
  return ctx;
}

void CTUI_setTargetFrameNs(CTUI_Context *ctx, uint64_t target_frame_ns) {
  ctx->_target_frame_ns = target_frame_ns;
}

uint64_t CTUI_getTargetFrameNs(CTUI_Context *ctx) {
  return ctx->_target_frame_ns;
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
  return 1;
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

void CTUI_hideWindow(CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->hideWindow != NULL) {
    console->_platform->hideWindow(console);
  }
}

void CTUI_showWindow(CTUI_Console *console) {
  if (console->_platform != NULL && console->_platform->showWindow != NULL) {
    console->_platform->showWindow(console);
  }
}

void CTUI_setWindowedTileWh(CTUI_Console *console,
                            CTUI_SVector2 console_tile_wh) {
  if (console->_platform != NULL &&
      console->_platform->setWindowedTileWh != NULL) {
    console->_platform->setWindowedTileWh(console, console_tile_wh);
  }
}

void CTUI_setWindowedFullscreen(CTUI_Console *console) {
  if (console->_platform != NULL &&
      console->_platform->setWindowedFullscreen != NULL) {
    console->_platform->setWindowedFullscreen(console);
  }
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
  CTUI_ConsoleLayer *layer = CTUI_getConsoleLayer(console, layer_i);
  if (layer == NULL) {
    return;
  }
  // Ensure non-zero divisors
  if (tile_div_wh.x == 0)
    tile_div_wh.x = 1;
  if (tile_div_wh.y == 0)
    tile_div_wh.y = 1;
  // Clear layer data
  layer->_tile_div_wh = tile_div_wh;
}

const CTUI_Font *CTUI_getFont(const CTUI_ConsoleLayer *layer) {
  return layer->_font;
}

void CTUI_setFont(CTUI_ConsoleLayer *layer, CTUI_Font *font) {
  layer->_font = font;
}

CTUI_SVector2 CTUI_getConsoleTileWh(const CTUI_Console *console) {
  return console->_console_tile_wh;
}

size_t CTUI_getConsoleLayerCount(const CTUI_Console *console) {
  return console->_layer_count;
}

CTUI_ConsoleLayer *CTUI_getConsoleLayer(CTUI_Console *console, size_t layer_i) {
  if (layer_i >= console->_layer_count || console->_layers == NULL) {
    return NULL;
  }
  // Use layer_size for proper indexing with platform-specific layer structs
  return (CTUI_ConsoleLayer *)((char *)console->_layers +
                                layer_i * console->_layer_size);
}

int CTUI_getConsoleIsRealTerminal(const CTUI_Console *console) {
  return console->_is_real_terminal;
}
