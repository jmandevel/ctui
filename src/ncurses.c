#include <ctui/ctui.h>
#include <locale.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CTUI_NcursesConsole {
  CTUI_Console base;
  CTUI_DVector2 last_mouse_pos;
  int mouse_buttons[8];
} CTUI_NcursesConsole;

static void CTUI_destroyConsoleNcurses(CTUI_Console *console) {
  endwin();
  if (console->_layers != NULL) {
    for (size_t i = 0; i < console->_layer_count; i++) {
      if (console->_layers[i]._tiles != NULL) {
        free(console->_layers[i]._tiles);
      }
    }
    free(console->_layers);
    console->_layers = NULL;
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

static void CTUI_refreshNcurses(CTUI_Console *console) {
  clear();
  if (console->_fill_bg_set) {
    int bgidx;
    switch (console->_effective_color_mode) {
    case CTUIC_ANSI256:
      bgidx = (int)CTUI_convertToAnsi256(console->_fill_bg_color);
      break;
    case CTUIC_ANSI16:
      bgidx = (int)CTUI_convertToAnsi16(console->_fill_bg_color);
      break;
    default:
      bgidx = (int)CTUI_convertToAnsi8(console->_fill_bg_color);
      break;
    }
    if (has_colors()) {
      init_pair(2, 0, bgidx);
      attron(COLOR_PAIR(2));
    }
    for (size_t y = 0; y < console->_console_tile_wh.y; y++) {
      for (size_t x = 0; x < console->_console_tile_wh.x; x++) {
        mvaddch(y, x, ' ');
      }
    }
    if (has_colors()) {
      attroff(COLOR_PAIR(2));
    }
  }
  for (size_t layer_i = 0; layer_i < console->_layer_count; layer_i++) {
    CTUI_ConsoleLayer *layer = &console->_layers[layer_i];
    for (size_t tile_i = 0; tile_i < layer->_tiles_count; tile_i++) {
      CTUI_ConsoleTile *tile = &layer->_tiles[tile_i];
      int fg, bg;
      switch (console->_effective_color_mode) {
      case CTUIC_ANSI256:
        fg = (int)CTUI_convertToAnsi256(tile->_fg);
        bg = (int)CTUI_convertToAnsi256(tile->_bg);
        break;
      case CTUIC_ANSI16:
        fg = (int)CTUI_convertToAnsi16(tile->_fg);
        bg = (int)CTUI_convertToAnsi16(tile->_bg);
        break;
      default:
        fg = (int)CTUI_convertToAnsi8(tile->_fg);
        bg = (int)CTUI_convertToAnsi8(tile->_bg);
        break;
      }
      if (has_colors()) {
        init_pair(1, fg, bg);
        attron(COLOR_PAIR(1));
      }
      mvaddch(tile->_pos_xy.y, tile->_pos_xy.x, (wchar_t)tile->_codepoint);
      if (has_colors()) {
        attroff(COLOR_PAIR(1));
      }
    }
  }
  refresh();
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
      CTUI_Event ev = {0};
      ev.type = CTUI_EVENT_RESIZE;
      ev.console = console;
      ev.data.resize.console_tile_wh = console->_console_tile_wh;
      CTUI_pushEvent(console->_ctx, &ev);
      CTUI_clear(console);
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
    .resize = NULL,
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
    .setWindowedFullscreen = NULL};

CTUI_Console *CTUI_createNcursesRealTerminal(CTUI_Context *ctx, int layer_count,
                                             CTUI_ColorMode color_mode) {
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
  CTUI_ColorMode best_mode = CTUIC_ANSI8;
  if (has_colors()) {
    start_color();
    if (COLORS >= 256) {
      best_mode = CTUIC_ANSI256;
      use_default_colors();
    } else if (COLORS >= 16) {
      best_mode = CTUIC_ANSI16;
    } else if (COLORS >= 8) {
      best_mode = CTUIC_ANSI8;
    }
  }
  if (best_mode > color_mode)
    best_mode = color_mode;
  int maxc = (best_mode == CTUIC_ANSI256)  ? 256
             : (best_mode == CTUIC_ANSI16) ? 16
                                           : 8;
  for (int i = 0; i < maxc && i < COLORS; ++i) {
    init_pair(i, i, -1);
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
  console->_layers = calloc(layer_count, sizeof(CTUI_ConsoleLayer));
  for (int layer_i = 0; layer_i < layer_count; layer_i++) {
    CTUI_ConsoleLayer *layer = &console->_layers[layer_i];
    layer->_tile_div_wh = (CTUI_DVector2){.x = 1.0, .y = 1.0};
    layer->_tiles = NULL;
    layer->_tiles_count = 0;
    layer->_tiles_capacity = 0;
  }
  if (ctx->_first_console != NULL) {
    ctx->_first_console->_prev = console;
  }
  console->_next = ctx->_first_console;
  ctx->_first_console = console;
  return console;
}
