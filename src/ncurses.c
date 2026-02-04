#include <ctui/ctui.h>
#include <locale.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum CTUI_DrawCommandKind {
  CTUI_DRAW_FILL,
  CTUI_DRAW_TILE
} CTUI_DrawCommandKind;

typedef struct CTUI_DrawCommand {
  CTUI_DrawCommandKind _kind;
  uint32_t _codepoint;
  CTUI_Color _fg;
  CTUI_Color _bg;
  union {
    CTUI_IVector2 _tile_pos;
  } _data;
} CTUI_DrawCommand;

typedef struct CTUI_ActualTile {
  uint32_t _codepoint;
  CTUI_Color _fg;
  CTUI_Color _bg;
} CTUI_ActualTile;

typedef struct CTUI_NcursesConsoleLayer {
  CTUI_ConsoleLayer base;
  CTUI_DrawCommand *_commands;
  size_t _command_count;
  size_t _command_capacity;
} CTUI_NcursesConsoleLayer;

typedef struct CTUI_NcursesConsole {
  CTUI_Console base;
  CTUI_DVector2 last_mouse_pos;
  int mouse_buttons[8];
  CTUI_ActualTile *_cur_buffer;  
  CTUI_ActualTile *_prev_buffer; 
  size_t _buffer_width;
  size_t _buffer_height;
} CTUI_NcursesConsole;

static CTUI_NcursesConsoleLayer *CTUI_getNcursesLayer(CTUI_Console *console,
                                                       size_t layer_i) {
  if (layer_i >= console->_layer_count)
    return NULL;
  return (CTUI_NcursesConsoleLayer *)((char *)console->_layers +
                                       layer_i * console->_layer_size);
}

static void CTUI_destroyConsoleNcurses(CTUI_Console *console) {
  CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
  endwin();
  // Free draw command arrays in layers
  if (console->_layers != NULL) {
    for (size_t i = 0; i < console->_layer_count; i++) {
      CTUI_NcursesConsoleLayer *layer = CTUI_getNcursesLayer(console, i);
      if (layer->_commands != NULL) {
        free(layer->_commands);
      }
    }
    free(console->_layers);
    console->_layers = NULL;
  }
  // Free double buffers
  if (ncurses_console->_cur_buffer != NULL) {
    free(ncurses_console->_cur_buffer);
  }
  if (ncurses_console->_prev_buffer != NULL) {
    free(ncurses_console->_prev_buffer);
  }
  free(console);
}

static int CTUI_translateKeyNcurses(int ch) {
  switch (ch) {
  case KEY_UP:
    return CTUIK_UP;
  case KEY_DOWN:
    return CTUIK_DOWN;
  case KEY_LEFT:
    return CTUIK_LEFT;
  case KEY_RIGHT:
    return CTUIK_RIGHT;
  case 10:
    return CTUIK_ENTER;
  case 27:
    return CTUIK_ESCAPE;
  case 9:
    return CTUIK_TAB;
  case KEY_BACKSPACE:
    return CTUIK_BACKSPACE;
  default:
    if (ch >= 32 && ch <= 126)
      return ch;
    return -1;
  }
}

static int CTUI_actualTilesEqual(const CTUI_ActualTile *a, const CTUI_ActualTile *b) {
  return memcmp(a, b, sizeof(CTUI_ActualTile)) == 0;
}

static CTUI_DrawCommand *CTUI_addDrawCommand(CTUI_NcursesConsoleLayer *layer) {
  if (layer->_command_count >= layer->_command_capacity) {
    size_t new_capacity = layer->_command_capacity == 0 ? 64 : layer->_command_capacity * 2;
    CTUI_DrawCommand *new_commands = realloc(layer->_commands, new_capacity * sizeof(CTUI_DrawCommand));
    if (new_commands == NULL) {
      return NULL; // Allocation failed
    }
    layer->_commands = new_commands;
    layer->_command_capacity = new_capacity;
  }
  return &layer->_commands[layer->_command_count++];
}

static void CTUI_pushCodepointNcurses(CTUI_ConsoleLayer *base_layer,
                                       uint32_t codepoint, CTUI_IVector2 pos_xy,
                                       CTUI_Color fg, CTUI_Color bg) {
  CTUI_NcursesConsoleLayer *layer = (CTUI_NcursesConsoleLayer *)base_layer;
  
  CTUI_DrawCommand *cmd = CTUI_addDrawCommand(layer);
  if (cmd == NULL) return;
  
  cmd->_kind = CTUI_DRAW_TILE;
  cmd->_codepoint = codepoint;
  cmd->_fg = fg;
  cmd->_bg = bg;
  cmd->_data._tile_pos = pos_xy;
}

static void CTUI_fillNcurses(CTUI_ConsoleLayer *base_layer, uint32_t codepoint,
                              CTUI_Color fg, CTUI_Color bg) {
  CTUI_NcursesConsoleLayer *layer = (CTUI_NcursesConsoleLayer *)base_layer;
  CTUI_DrawCommand *cmd = CTUI_addDrawCommand(layer);
  if (cmd == NULL) return;
  
  cmd->_kind = CTUI_DRAW_FILL;
  cmd->_codepoint = codepoint;
  cmd->_fg = fg;
  cmd->_bg = bg;
}

static void CTUI_refreshNcurses(CTUI_Console *console) {
  CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
  size_t buf_width = ncurses_console->_buffer_width;
  size_t buf_height = ncurses_console->_buffer_height;
  CTUI_ActualTile *cur_buf = ncurses_console->_cur_buffer;
  CTUI_ActualTile *prev_buf = ncurses_console->_prev_buffer;  
  for (size_t layer_i = 0; layer_i < console->_layer_count; layer_i++) {
    CTUI_NcursesConsoleLayer *layer = CTUI_getNcursesLayer(console, layer_i);
    for (size_t cmd_i = 0; cmd_i < layer->_command_count; cmd_i++) {
      CTUI_DrawCommand *cmd = &layer->_commands[cmd_i];
      switch (cmd->_kind) {
      case CTUI_DRAW_FILL:
        for (size_t i = 0; i < buf_width * buf_height; i++) {
          cur_buf[i]._codepoint = cmd->_codepoint;
          cur_buf[i]._fg = cmd->_fg;
          cur_buf[i]._bg = cmd->_bg;
        }
        break;
      case CTUI_DRAW_TILE: {
        CTUI_IVector2 pos = cmd->_data._tile_pos;
        if (pos.x < 0 || pos.y < 0)
          continue;
        if (pos.x >= buf_width || pos.y >= buf_height)
          continue;
        size_t idx = pos.y * buf_width + pos.x;
        cur_buf[idx]._codepoint = cmd->_codepoint;
        cur_buf[idx]._fg = cmd->_fg;
        cur_buf[idx]._bg = cmd->_bg;
        break;
      }
      }
    }
    layer->_command_count = 0;
  }
  for (size_t y = 0; y < buf_height; y++) {
    for (size_t x = 0; x < buf_width; x++) {
      size_t idx = y * buf_width + x;
      CTUI_ActualTile *cur = &cur_buf[idx];
      CTUI_ActualTile *prev = &prev_buf[idx];
      if (CTUI_actualTilesEqual(cur, prev))
        continue;
      memcpy(prev, cur, sizeof(CTUI_ActualTile));
      int fg, bg, pair_i;
      switch (console->_effective_color_mode) {
      case CTUI_COLORMODE_ANSI8:
        fg = (int)CTUI_convertToAnsi8(cur->_fg);
        bg = (int)CTUI_convertToAnsi8(cur->_bg);
        if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
          pair_i = 0;
        } else {
          pair_i = (bg * 8) + fg;
        }
        attron(COLOR_PAIR(pair_i));
      case CTUI_COLORMODE_ANSI16:
        fg = (int)CTUI_convertToAnsi16(cur->_fg);
        bg = (int)CTUI_convertToAnsi16(cur->_bg);
        if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
          pair_i = 0;
        } else {
          pair_i = (bg * 16) + fg;
        }
        attron(COLOR_PAIR(pair_i));
        break;
      case CTUI_COLORMODE_ANSI256:
        fg = (int)CTUI_convertToAnsi256(cur->_fg);
        bg = (int)CTUI_convertToAnsi256(cur->_bg);
        if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
          pair_i = 0;
        } else {
          pair_i = (bg * 256) + fg;
        }
        attron(COLOR_PAIR(pair_i));
        break;
      default:
        break;
      }
      mvaddch(y, x, (wchar_t)cur->_codepoint);
      if (console->_effective_color_mode != CTUI_COLORMODE_NO_COLORS) {
        attroff(COLOR_PAIR(pair_i));
      }
    }
  }
  refresh();
}

// Resize handler to reallocate double buffers when terminal size changes
static void CTUI_resizeNcurses(CTUI_Console *console,
                                CTUI_SVector2 console_tile_wh) {
  CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
  size_t new_width = console_tile_wh.x;
  size_t new_height = console_tile_wh.y;
  
  // Only reallocate if size changed
  if (ncurses_console->_buffer_width != new_width || 
      ncurses_console->_buffer_height != new_height) {
    
    // Free old buffers
    if (ncurses_console->_cur_buffer != NULL) {
      free(ncurses_console->_cur_buffer);
    }
    if (ncurses_console->_prev_buffer != NULL) {
      free(ncurses_console->_prev_buffer);
    }
    
    // Allocate new buffers
    size_t buf_size = new_width * new_height;
    ncurses_console->_cur_buffer = calloc(buf_size, sizeof(CTUI_ActualTile));
    ncurses_console->_prev_buffer = calloc(buf_size, sizeof(CTUI_ActualTile));
    ncurses_console->_buffer_width = new_width;
    ncurses_console->_buffer_height = new_height;
    
    // Initialize both buffers to force full redraw
    // (calloc already zeroed them, which will differ from any real content)
  }
}

static void CTUI_pollEventsNcurses(CTUI_Console *console) {
  nodelay(stdscr, TRUE);
  int ch = getch();
  while (ch != ERR) {
    // Handle resize event
    if (ch == KEY_RESIZE) {
      int rows, cols;
      getmaxyx(stdscr, rows, cols);
      console->_console_tile_wh.x = cols;
      console->_console_tile_wh.y = rows;
      
      // Resize double buffers
      CTUI_resizeNcurses(console, console->_console_tile_wh);
      
      CTUI_Event ev = {0};
      ev.type = CTUI_EVENT_RESIZE;
      ev.console = console;
      ev.data.resize.console_tile_wh = console->_console_tile_wh;
      CTUI_pushEvent(console->_ctx, &ev);
      ch = getch();
      continue;
    }
    int ctui_key = CTUI_translateKeyNcurses(ch);
    if (ctui_key != -1) {
      CTUI_Event ev = {0};
      ev.type = CTUI_EVENT_KEY;
      ev.console = console;
      ev.data.key.key = (CTUI_Key)ctui_key;
      ev.data.key.scancode = 0;
      ev.data.key.action = CTUIA_PRESS;
      ev.data.key.mods = 0;
      CTUI_pushEvent(console->_ctx, &ev);
    }
#ifdef NCURSES_MOUSE_VERSION
    if (ch == KEY_MOUSE) {
      MEVENT event;
      if (getmouse(&event) == OK) {
        CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
        ncurses_console->last_mouse_pos.x = (double)event.x;
        ncurses_console->last_mouse_pos.y = (double)event.y;
        CTUI_Event mev = {0};
        mev.type = CTUI_EVENT_MOUSE_BUTTON;
        mev.console = console;
        if (event.bstate & BUTTON1_PRESSED) {
          mev.data.mouse_button.button = CTUIMB_LEFT;
          mev.data.mouse_button.action = CTUIA_PRESS;
          ncurses_console->mouse_buttons[CTUIMB_LEFT] = 1;
          CTUI_pushEvent(console->_ctx, &mev);
        } else if (event.bstate & BUTTON1_RELEASED) {
          mev.data.mouse_button.button = CTUIMB_LEFT;
          mev.data.mouse_button.action = CTUIA_RELEASE;
          ncurses_console->mouse_buttons[CTUIMB_LEFT] = 0;
          CTUI_pushEvent(console->_ctx, &mev);
        }
        if (event.bstate & BUTTON2_PRESSED) {
          mev.data.mouse_button.button = CTUIMB_MIDDLE;
          mev.data.mouse_button.action = CTUIA_PRESS;
          ncurses_console->mouse_buttons[CTUIMB_MIDDLE] = 1;
          CTUI_pushEvent(console->_ctx, &mev);
        } else if (event.bstate & BUTTON2_RELEASED) {
          mev.data.mouse_button.button = CTUIMB_MIDDLE;
          mev.data.mouse_button.action = CTUIA_RELEASE;
          ncurses_console->mouse_buttons[CTUIMB_MIDDLE] = 0;
          CTUI_pushEvent(console->_ctx, &mev);
        }
        if (event.bstate & BUTTON3_PRESSED) {
          mev.data.mouse_button.button = CTUIMB_RIGHT;
          mev.data.mouse_button.action = CTUIA_PRESS;
          ncurses_console->mouse_buttons[CTUIMB_RIGHT] = 1;
          CTUI_pushEvent(console->_ctx, &mev);
        } else if (event.bstate & BUTTON3_RELEASED) {
          mev.data.mouse_button.button = CTUIMB_RIGHT;
          mev.data.mouse_button.action = CTUIA_RELEASE;
          ncurses_console->mouse_buttons[CTUIMB_RIGHT] = 0;
          CTUI_pushEvent(console->_ctx, &mev);
        }
        CTUI_Event cev = {0};
        cev.type = CTUI_EVENT_CURSOR_POS;
        cev.console = console;
        cev.data.cursor_pos.viewport_xy.x =
            0.0; // NOTE: viewport pos is (0,0) because has no viewport
        cev.data.cursor_pos.viewport_xy.y = 0.0;
        cev.data.cursor_pos.tile_xy.x = (double)event.x;
        cev.data.cursor_pos.tile_xy.y = (double)event.y;
        CTUI_pushEvent(console->_ctx, &cev);
      }
    }
#endif
    ch = getch();
  }
}

static CTUI_DVector2 CTUI_getCursorTilePosNcurses(CTUI_Console *console) {
  CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
  return ncurses_console->last_mouse_pos;
}

static int CTUI_getMouseButtonNcurses(CTUI_Console *console, int button) {
  if (button < 0 || button > 7) {
    return 0;
  }
  CTUI_NcursesConsole *ncurses_console = (CTUI_NcursesConsole *)console;
  return ncurses_console->mouse_buttons[button];
}

static CTUI_PlatformVtable CTUI_PLATFORM_VTABLE_NCURSES = {
    .is_resizable = 1,
    .destroy = CTUI_destroyConsoleNcurses,
    .resize = CTUI_resizeNcurses,
    .refresh = CTUI_refreshNcurses,
    .pollEvents = CTUI_pollEventsNcurses,
    .getCursorViewportPos = NULL,
    .getCursorTilePos = CTUI_getCursorTilePosNcurses,
    .getMouseButton = CTUI_getMouseButtonNcurses,
    .getKeyState = NULL,
    .transformViewport = NULL,
    .resetViewport = NULL,
    .setWindowPixelWh = NULL,
    .getWindowPixelWh = NULL,
    .setViewportTileWh = NULL,
    .fitWindowPixelWhToViewportTileWh = NULL,
    .fitViewportTileWhToWindowPixelWh = NULL,
    .setWindowResizable = NULL,
    .getWindowResizable = NULL,
    .getIsFullscreen = NULL,
    .setWindowDecorated = NULL,
    .getWindowDecorated = NULL,
    .setWindowFloating = NULL,
    .getWindowFloating = NULL,
    .minimizeWindow = NULL,
    .maximizeWindow = NULL,
    .restoreWindow = NULL,
    .getWindowMinimized = NULL,
    .getWindowMaximized = NULL,
    .focusWindow = NULL,
    .getWindowFocused = NULL,
    .requestWindowAttention = NULL,
    .setWindowOpacity = NULL,
    .getWindowOpacity = NULL,
    .hideWindow = NULL,
    .showWindow = NULL,
    .setWindowedTileWh = NULL,
    .setWindowedFullscreen = NULL,
    .layer_size = sizeof(CTUI_NcursesConsoleLayer),
    .pushCodepoint = CTUI_pushCodepointNcurses,
    .fill = CTUI_fillNcurses};

CTUI_Console *CTUI_createNcursesRealTerminal(CTUI_Context *ctx, int layer_count) {
  if (!CTUI_getHasRealTerminal()) {
    return NULL;
  }
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
#ifdef NCURSES_MOUSE_VERSION
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
#endif
  CTUI_ColorMode best_mode;
  start_color();
  if (!has_colors()) {
    best_mode = CTUI_COLORMODE_NO_COLORS;
  } else if (COLORS >= 256 && COLOR_PAIRS >= (256 * 256)) {
    best_mode = CTUI_COLORMODE_ANSI256;
    int pair_i = 0;
    for (int color_i1 = 0; color_i1 < 256; color_i1++) {
      for (int color_i2 = 0; color_i2 < 256; color_i2++) {
        if (color_i1 == COLOR_WHITE && color_i2 == COLOR_BLACK) {
          init_pair(pair_i++, 0, 0);
          continue;
        }
        init_pair(pair_i++, color_i1, color_i2);
      }
    }
  } else if (COLORS >= 16 && COLOR_PAIRS >= (16 * 16)) {
    best_mode = CTUI_COLORMODE_ANSI16;
    int pair_i = 0;
    for (int color_i1 = 0; color_i1 < 16; color_i1++) {
      for (int color_i2 = 0; color_i2 < 16; color_i2++) {
        if (color_i1 == COLOR_WHITE && color_i2 == COLOR_BLACK) {
          init_pair(pair_i++, 0, 0);
          continue;
        }
        init_pair(pair_i++, color_i1, color_i2);
      }
    }
  } else if (COLORS >= 8 && COLOR_PAIRS >= (8 * 8)) {
    best_mode = CTUI_COLORMODE_ANSI8;
    int pair_i = 0;
    for (int color_i1 = 0; color_i1 < 8; color_i1++) {
      for (int color_i2 = 0; color_i2 < 8; color_i2++) {
        if (color_i1 == COLOR_WHITE && color_i2 == COLOR_BLACK) {
          init_pair(pair_i++, 0, 0);
          continue;
        }
        init_pair(pair_i++, color_i1, color_i2);
      }
    }
  } else {
    best_mode = CTUI_COLORMODE_NO_COLORS;
  }
  CTUI_NcursesConsole *ncurses_console = calloc(1, sizeof(CTUI_NcursesConsole));
  CTUI_Console *console = &ncurses_console->base;
  console->_platform = &CTUI_PLATFORM_VTABLE_NCURSES;
  console->_ctx = ctx;
  console->_is_real_terminal = 1;
  console->_effective_color_mode = best_mode;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  console->_console_tile_wh.x = cols;
  console->_console_tile_wh.y = rows;
  console->_layer_count = layer_count;
  console->_layer_size = sizeof(CTUI_NcursesConsoleLayer);
  console->_layers = calloc(layer_count, sizeof(CTUI_NcursesConsoleLayer));
  console->_effective_color_mode = best_mode;
  for (int layer_i = 0; layer_i < layer_count; layer_i++) {
    CTUI_NcursesConsoleLayer *layer = CTUI_getNcursesLayer(console, layer_i);
    layer->base._console = console; // Set back-reference
    layer->base._tile_div_wh = (CTUI_DVector2){.x = 1.0, .y = 1.0};
    layer->base._font = NULL;
    layer->_commands = NULL;
    layer->_command_count = 0;
    layer->_command_capacity = 0;
  }
  size_t buf_size = cols * rows;
  ncurses_console->_cur_buffer = calloc(buf_size, sizeof(CTUI_ActualTile));
  ncurses_console->_prev_buffer = calloc(buf_size, sizeof(CTUI_ActualTile));
  ncurses_console->_buffer_width = cols;
  ncurses_console->_buffer_height = rows;
  if (ctx->_first_console != NULL) {
    ctx->_first_console->_prev = console;
  }
  console->_next = ctx->_first_console;
  ctx->_first_console = console;
  return console;
}
