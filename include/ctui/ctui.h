#ifndef CTUI_H
#define CTUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct CTUI_IVector2 {
  int x;
  int y;
} CTUI_IVector2;

typedef struct CTUI_SVector2 {
  size_t x;
  size_t y;
} CTUI_SVector2;

typedef struct CTUI_FVector2 {
  float x;
  float y;
} CTUI_FVector2;

typedef struct CTUI_DVector2 {
  double x;
  double y;
} CTUI_DVector2;

typedef struct CTUI_Matrix4x4 {
  float m[16];
} CTUI_Matrix4x4;

typedef struct CTUI_Color32 {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} CTUI_Color32;

typedef struct CTUI_Color {
  uint8_t palette8;
  uint8_t palette16;
  uint8_t palette256;
  CTUI_Color32 full;
} CTUI_Color;

typedef enum CTUI_ColorMode {
  CTUI_COLORMODE_8,
  CTUI_COLORMODE_16,
  CTUI_COLORMODE_256,
  CTUI_COLORMODE_FULL
} CTUI_ColorMode;

typedef struct CTUI_Console CTUI_Console;

typedef void (*CTUI_DestroyCallback)(CTUI_Console *console);
typedef void (*CTUI_ResizeCallback)(CTUI_Console *console,
                                    CTUI_SVector2 console_tile_wh);
typedef void (*CTUI_RefreshCallback)(CTUI_Console *console);
typedef void (*CTUI_PollEventsCallback)(CTUI_Console *console);
typedef CTUI_DVector2 (*CTUI_GetCursorViewportPosCallback)(
    CTUI_Console *console);
typedef CTUI_DVector2 (*CTUI_GetCursorTilePosCallback)(CTUI_Console *console);
typedef int (*CTUI_GetMouseButtonCallback)(CTUI_Console *console, int button);
typedef int (*CTUI_GetKeyStateCallback)(CTUI_Console *console, int key);
typedef void (*CTUI_TransformViewportCallback)(CTUI_Console *console,
                                               CTUI_FVector2 translation,
                                               CTUI_FVector2 scale);
typedef void (*CTUI_ResetViewportCallback)(CTUI_Console *console);
typedef void (*CTUI_SetWindowPixelWhCallback)(CTUI_Console *console,
                                              CTUI_IVector2 pixel_wh);
typedef CTUI_IVector2 (*CTUI_GetWindowPixelWhCallback)(CTUI_Console *console);
typedef void (*CTUI_SetViewportTileWhCallback)(CTUI_Console *console,
                                               CTUI_SVector2 tile_wh);
typedef void (*CTUI_FitWindowPixelWhToViewportTileWhCallback)(
    CTUI_Console *console);
typedef void (*CTUI_FitViewportTileWhToWindowPixelWhCallback)(
    CTUI_Console *console);
typedef void (*CTUI_SetWindowResizableCallback)(CTUI_Console *console,
                                                int resizable);
typedef int (*CTUI_GetWindowResizableCallback)(CTUI_Console *console);
typedef int (*CTUI_GetIsFullscreenCallback)(CTUI_Console *console);
typedef void (*CTUI_SetWindowDecoratedCallback)(CTUI_Console *console,
                                                int decorated);
typedef int (*CTUI_GetWindowDecoratedCallback)(CTUI_Console *console);
typedef void (*CTUI_SetWindowFloatingCallback)(CTUI_Console *console,
                                               int floating);
typedef int (*CTUI_GetWindowFloatingCallback)(CTUI_Console *console);
typedef void (*CTUI_MinimizeWindowCallback)(CTUI_Console *console);
typedef void (*CTUI_MaximizeWindowCallback)(CTUI_Console *console);
typedef void (*CTUI_RestoreWindowCallback)(CTUI_Console *console);
typedef int (*CTUI_GetWindowMinimizedCallback)(CTUI_Console *console);
typedef int (*CTUI_GetWindowMaximizedCallback)(CTUI_Console *console);
typedef void (*CTUI_FocusWindowCallback)(CTUI_Console *console);
typedef int (*CTUI_GetWindowFocusedCallback)(CTUI_Console *console);
typedef void (*CTUI_RequestWindowAttentionCallback)(CTUI_Console *console);
typedef void (*CTUI_SetWindowOpacityCallback)(CTUI_Console *console,
                                              float opacity);
typedef float (*CTUI_GetWindowOpacityCallback)(CTUI_Console *console);

typedef struct CTUI_TuiPlatform {
  int is_resizable;
  CTUI_DestroyCallback destroy;
  CTUI_ResizeCallback resize;
  CTUI_RefreshCallback refresh;
  CTUI_PollEventsCallback pollEvents;
  CTUI_GetCursorViewportPosCallback getCursorViewportPos;
  CTUI_GetCursorTilePosCallback getCursorTilePos;
  CTUI_GetMouseButtonCallback getMouseButton;
  CTUI_GetKeyStateCallback getKeyState;
  CTUI_TransformViewportCallback transformViewport;
  CTUI_ResetViewportCallback resetViewport;
  CTUI_SetWindowPixelWhCallback setWindowPixelWh;
  CTUI_GetWindowPixelWhCallback getWindowPixelWh;
  CTUI_SetViewportTileWhCallback setViewportTileWh;
  CTUI_FitWindowPixelWhToViewportTileWhCallback
      fitWindowPixelWhToViewportTileWh;
  CTUI_FitViewportTileWhToWindowPixelWhCallback
      fitViewportTileWhToWindowPixelWh;
  CTUI_SetWindowResizableCallback setWindowResizable;
  CTUI_GetWindowResizableCallback getWindowResizable;
  CTUI_GetIsFullscreenCallback getIsFullscreen;
  CTUI_SetWindowDecoratedCallback setWindowDecorated;
  CTUI_GetWindowDecoratedCallback getWindowDecorated;
  CTUI_SetWindowFloatingCallback setWindowFloating;
  CTUI_GetWindowFloatingCallback getWindowFloating;
  CTUI_MinimizeWindowCallback minimizeWindow;
  CTUI_MaximizeWindowCallback maximizeWindow;
  CTUI_RestoreWindowCallback restoreWindow;
  CTUI_GetWindowMinimizedCallback getWindowMinimized;
  CTUI_GetWindowMaximizedCallback getWindowMaximized;
  CTUI_FocusWindowCallback focusWindow;
  CTUI_GetWindowFocusedCallback getWindowFocused;
  CTUI_RequestWindowAttentionCallback requestWindowAttention;
  CTUI_SetWindowOpacityCallback setWindowOpacity;
  CTUI_GetWindowOpacityCallback getWindowOpacity;
} CTUI_TuiPlatform;

typedef enum CTUI_Key {
  CTUIK_SPACE = 32,
  CTUIK_APOSTROPHE = 39,
  CTUIK_COMMA = 44,
  CTUIK_MINUS = 45,
  CTUIK_PERIOD = 46,
  CTUIK_SLASH = 47,
  CTUIK_NUM0 = 48,
  CTUIK_NUM1 = 49,
  CTUIK_NUM2 = 50,
  CTUIK_NUM3 = 51,
  CTUIK_NUM4 = 52,
  CTUIK_NUM5 = 53,
  CTUIK_NUM6 = 54,
  CTUIK_NUM7 = 55,
  CTUIK_NUM8 = 56,
  CTUIK_NUM9 = 57,
  CTUIK_SEMICOLON = 59,
  CTUIK_EQUAL = 61,
  CTUIK_A = 65,
  CTUIK_B = 66,
  CTUIK_C = 67,
  CTUIK_D = 68,
  CTUIK_E = 69,
  CTUIK_F = 70,
  CTUIK_G = 71,
  CTUIK_H = 72,
  CTUIK_I = 73,
  CTUIK_J = 74,
  CTUIK_K = 75,
  CTUIK_L = 76,
  CTUIK_M = 77,
  CTUIK_N = 78,
  CTUIK_O = 79,
  CTUIK_P = 80,
  CTUIK_Q = 81,
  CTUIK_R = 82,
  CTUIK_S = 83,
  CTUIK_T = 84,
  CTUIK_U = 85,
  CTUIK_V = 86,
  CTUIK_W = 87,
  CTUIK_X = 88,
  CTUIK_Y = 89,
  CTUIK_Z = 90,
  CTUIK_LEFT_BRACKET = 91,
  CTUIK_BACKSLASH = 92,
  CTUIK_RIGHT_BRACKET = 93,
  CTUIK_GRAVE_ACCENT = 96,
  CTUIK_WORLD_1 = 161,
  CTUIK_WORLD_2 = 162,
  CTUIK_ESCAPE = 256,
  CTUIK_ENTER = 257,
  CTUIK_TAB = 258,
  CTUIK_BACKSPACE = 259,
  CTUIK_INSERT = 260,
  CTUIK_DELETE = 261,
  CTUIK_RIGHT = 262,
  CTUIK_LEFT = 263,
  CTUIK_DOWN = 264,
  CTUIK_UP = 265,
  CTUIK_PAGE_UP = 266,
  CTUIK_PAGE_DOWN = 267,
  CTUIK_HOME = 268,
  CTUIK_END = 269,
  CTUIK_CAPS_LOCK = 280,
  CTUIK_SCROLL_LOCK = 281,
  CTUIK_NUM_LOCK = 282,
  CTUIK_PRINT_SCREEN = 283,
  CTUIK_PAUSE = 284,
  CTUIK_F1 = 290,
  CTUIK_F2 = 291,
  CTUIK_F3 = 292,
  CTUIK_F4 = 293,
  CTUIK_F5 = 294,
  CTUIK_F6 = 295,
  CTUIK_F7 = 296,
  CTUIK_F8 = 297,
  CTUIK_F9 = 298,
  CTUIK_F10 = 299,
  CTUIK_F11 = 300,
  CTUIK_F12 = 301,
  CTUIK_F13 = 302,
  CTUIK_F14 = 303,
  CTUIK_F15 = 304,
  CTUIK_F16 = 305,
  CTUIK_F17 = 306,
  CTUIK_F18 = 307,
  CTUIK_F19 = 308,
  CTUIK_F20 = 309,
  CTUIK_F21 = 310,
  CTUIK_F22 = 311,
  CTUIK_F23 = 312,
  CTUIK_F24 = 313,
  CTUIK_F25 = 314,
  CTUIK_KP_0 = 320,
  CTUIK_KP_1 = 321,
  CTUIK_KP_2 = 322,
  CTUIK_KP_3 = 323,
  CTUIK_KP_4 = 324,
  CTUIK_KP_5 = 325,
  CTUIK_KP_6 = 326,
  CTUIK_KP_7 = 327,
  CTUIK_KP_8 = 328,
  CTUIK_KP_9 = 329,
  CTUIK_KP_DECIMAL = 330,
  CTUIK_KP_DIVIDE = 331,
  CTUIK_KP_MULTIPLY = 332,
  CTUIK_KP_SUBTRACT = 333,
  CTUIK_KP_ADD = 334,
  CTUIK_KP_ENTER = 335,
  CTUIK_KP_EQUAL = 336,
  CTUIK_LEFT_SHIFT = 340,
  CTUIK_LEFT_CONTROL = 341,
  CTUIK_LEFT_ALT = 342,
  CTUIK_LEFT_SUPER = 343,
  CTUIK_RIGHT_SHIFT = 344,
  CTUIK_RIGHT_CONTROL = 345,
  CTUIK_RIGHT_ALT = 346,
  CTUIK_RIGHT_SUPER = 347,
  CTUIK_MENU = 348,
  CTUIK_LAST = CTUIK_MENU
} CTUI_Key;

typedef enum CTUI_MouseButton {
  CTUIMB_1 = 0,
  CTUIMB_2 = 1,
  CTUIMB_3 = 2,
  CTUIMB_4 = 3,
  CTUIMB_5 = 4,
  CTUIMB_6 = 5,
  CTUIMB_7 = 6,
  CTUIMB_8 = 7,
  CTUIMB_LAST = CTUIMB_8,
  CTUIMB_LEFT = CTUIMB_1,
  CTUIMB_RIGHT = CTUIMB_2,
  CTUIMB_MIDDLE = CTUIMB_3
} CTUI_MouseButton;

typedef enum CTUI_Action { CTUIA_RELEASE = 0, CTUIA_PRESS = 1 } CTUI_Action;

typedef enum CTUI_EventType {
  CTUI_EVENT_NONE = 0,
  CTUI_EVENT_KEY,
  CTUI_EVENT_MOUSE_BUTTON,
  CTUI_EVENT_CURSOR_POS,
  CTUI_EVENT_SCROLL,
  CTUI_EVENT_RESIZE,
  CTUI_EVENT_CLOSE
} CTUI_EventType;

typedef struct CTUI_Event {
  CTUI_EventType type;
  CTUI_Console *console;
  union {
    struct {
      CTUI_Key key;
      int scancode;
      CTUI_Action action;
      int mods;
    } key;
    struct {
      int button;
      int action;
      int mods;
    } mouse_button;
    struct {
      CTUI_DVector2 viewport_xy;
      CTUI_DVector2 tile_xy;
    } cursor_pos;
    struct {
      CTUI_DVector2 scroll_xy;
    } scroll;
    struct {
      CTUI_SVector2 console_tile_wh;
    } resize;
  } data;
} CTUI_Event;

typedef struct CTUI_Stpqp {
  // left x
  float s;
  // right x
  float t;
  // top y
  float p;
  // bottom y
  float q;
  // texture page
  float page;
} CTUI_Stpqp;

typedef struct CTUI_Glyph {
  // how many layer tiles wide and tall is this tile
  CTUI_SVector2 _tiles_wh;
  uint32_t _codepoint;
  CTUI_Stpqp _tex_coords;
} CTUI_Glyph;

typedef struct CTUI_Image {
  // pixels wide
  size_t _width;
  // pixels tall
  size_t _height;
  // texture page count
  size_t _pages;
  // pixels data array (8 bit channel RGBA)
  unsigned char *_pixels;
} CTUI_Image;

typedef struct CTUI_Context CTUI_Context;
typedef struct CTUI_Font CTUI_Font;

typedef struct CTUI_Font {
  CTUI_Image _image;
  size_t _max_map_offset;
  size_t _map_size;
  CTUI_Glyph *_glyph_map;
} CTUI_Font;

typedef struct CTUI_ConsoleTile {
  CTUI_SVector2 _pos_xy;
  uint32_t _codepoint;
  CTUI_Color _fg;
  CTUI_Color _bg;
} CTUI_ConsoleTile;

typedef struct CTUI_ConsoleLayer {
  CTUI_DVector2 _tile_div_wh;
  CTUI_Font *_font;
  size_t _tiles_capacity;
  size_t _tiles_count;
  CTUI_ConsoleTile *_tiles;
} CTUI_ConsoleLayer;

typedef struct CTUI_LayerInfo {
  CTUI_Font *font;
  CTUI_DVector2 tile_div_wh;
} CTUI_LayerInfo;

typedef struct CTUI_Context {
  CTUI_Console *_first_console;
  CTUI_Font *_first_font;
  CTUI_Event *_event_queue;
  size_t _event_queue_capacity;
  size_t _event_queue_count;
  size_t _event_queue_head;
} CTUI_Context;

typedef struct CTUI_Console {
  CTUI_TuiPlatform *_platform;
  CTUI_Context *_ctx;
  int _is_real_terminal;
  CTUI_Console *_next;
  CTUI_Console *_prev;
  CTUI_SVector2 _console_tile_wh;
  size_t _layer_count;
  CTUI_ConsoleLayer *_layers;
  CTUI_Color _fill_bg_color;
  int _fill_bg_set;
  CTUI_ColorMode _effective_color_mode;
} CTUI_Console;

int CTUI_getHasRealTerminal();

CTUI_Context *CTUI_createContext();

void CTUI_destroyContext(CTUI_Context *ctx);

int CTUI_hasConsole(CTUI_Context *ctx);

void CTUI_pollEvents(CTUI_Context *ctx);

void CTUI_pushEvent(CTUI_Context *ctx, CTUI_Event *event);

int CTUI_nextEvent(CTUI_Context *ctx, CTUI_Event *event);

CTUI_Font *CTUI_createFont(const char *ctuifont_path, const char **image_paths,
                           size_t image_count);

void CTUI_destroyFont(CTUI_Font *font);

CTUI_Glyph *CTUI_tryGetGlyph(CTUI_Font *font, uint32_t codepoint);

CTUI_SVector2 CTUI_getGlyphTilesWh(const CTUI_Glyph *glyph);

uint32_t CTUI_getGlyphCodepoint(const CTUI_Glyph *glyph);

CTUI_Stpqp CTUI_getGlyphTexCoords(const CTUI_Glyph *glyph);

size_t CTUI_getFontImageWidth(const CTUI_Font *font);

size_t CTUI_getFontImageHeight(const CTUI_Font *font);

size_t CTUI_getFontImagePages(const CTUI_Font *font);

CTUI_DVector2 CTUI_getLayerTileDivWh(const CTUI_ConsoleLayer *layer);

void CTUI_setLayerTileDivWh(CTUI_Console *console, size_t layer_i,
                            CTUI_DVector2 tile_div_wh);

const CTUI_Font *CTUI_getLayerFont(const CTUI_ConsoleLayer *layer);

void CTUI_setLayerFont(CTUI_Console *console, size_t layer_i, CTUI_Font *font);

size_t CTUI_getLayerTilesCount(const CTUI_ConsoleLayer *layer);

CTUI_SVector2 CTUI_getConsoleTileWh(const CTUI_Console *console);

size_t CTUI_getConsoleLayerCount(const CTUI_Console *console);

CTUI_ConsoleLayer *CTUI_getConsoleLayer(CTUI_Console *console, size_t layer_i);

CTUI_ColorMode CTUI_getConsoleColorMode(const CTUI_Console *console);

int CTUI_getConsoleIsRealTerminal(const CTUI_Console *console);

void CTUI_destroyConsole(CTUI_Console *console);

int CTUI_getIsWindow(CTUI_Console *console);

void CTUI_hideWindow(CTUI_Console *console);

void CTUI_setWindowedTileWh(CTUI_Console *console,
                            CTUI_SVector2 console_tile_wh);

void CTUI_setWindowedFullscreen(CTUI_Console *console);

int CTUI_getIsFullscreen(CTUI_Console *console);

void CTUI_setWindowPixelWh(CTUI_Console *console, CTUI_IVector2 pixel_wh);

CTUI_IVector2 CTUI_getWindowPixelWh(CTUI_Console *console);

void CTUI_setViewportTileWh(CTUI_Console *console, CTUI_SVector2 tile_wh);

void CTUI_fitWindowPixelWhToViewportTileWh(CTUI_Console *console);

void CTUI_fitViewportTileWhToWindowPixelWh(CTUI_Console *console);

void CTUI_setWindowResizable(CTUI_Console *console, int resizable);

int CTUI_getWindowResizable(CTUI_Console *console);

void CTUI_setWindowDecorated(CTUI_Console *console, int decorated);

int CTUI_getWindowDecorated(CTUI_Console *console);

void CTUI_setWindowFloating(CTUI_Console *console, int floating);

int CTUI_getWindowFloating(CTUI_Console *console);

void CTUI_minimizeWindow(CTUI_Console *console);

void CTUI_maximizeWindow(CTUI_Console *console);

void CTUI_restoreWindow(CTUI_Console *console);

int CTUI_getWindowMinimized(CTUI_Console *console);

int CTUI_getWindowMaximized(CTUI_Console *console);

void CTUI_focusWindow(CTUI_Console *console);

int CTUI_getWindowFocused(CTUI_Console *console);

void CTUI_requestWindowAttention(CTUI_Console *console);

void CTUI_setWindowOpacity(CTUI_Console *console, float opacity);

float CTUI_getWindowOpacity(CTUI_Console *console);

int CTUI_getHasViewport(CTUI_Console *console);

void CTUI_transformViewport(CTUI_Console *console, CTUI_FVector2 translation,
                            CTUI_FVector2 scale);

void CTUI_resetViewport(CTUI_Console *console);

CTUI_DVector2 CTUI_getCursorViewportPos(CTUI_Console *console);

CTUI_DVector2 CTUI_getCursorTilePos(CTUI_Console *console);

int CTUI_getMouseButton(CTUI_Console *console, CTUI_MouseButton button);

int CTUI_getKeyState(CTUI_Console *console, CTUI_Key key);

void CTUI_pushCodepoint(CTUI_ConsoleLayer *layer, uint32_t codepoint,
                        CTUI_IVector2 pos_xy, CTUI_Color fg, CTUI_Color bg);

void CTUI_fill(CTUI_Console *console, CTUI_Color bg);

void CTUI_refresh(CTUI_Console *console);

void CTUI_clear(CTUI_Console *console);

struct CTUI_Console *CTUI_createGlfwOpengl33FakeTerminal(
    struct CTUI_Context *context, CTUI_DVector2 tile_pixel_wh,
    size_t layer_count, const CTUI_LayerInfo *layer_infos,
    enum CTUI_ColorMode color_mode, const char *title);

CTUI_Console *CTUI_createNcursesRealTerminal(CTUI_Context *ctx, int layer_count,
                                             enum CTUI_ColorMode color_mode);

#ifdef __cplusplus
}
#endif

#endif