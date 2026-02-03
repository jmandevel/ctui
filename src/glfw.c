// GLFW Windowing Backend for CTUI
// This handles window management and input, delegating rendering to a CTUI_Renderer

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <ctui/ctui.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Identity matrix constant
static const float CTUI_IDENTITY_MATRIX[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static void CTUI_createTransformMatrix(float *out, float trans_x, float trans_y,
                                        float scale_x, float scale_y) {
  memcpy(out, CTUI_IDENTITY_MATRIX, sizeof(CTUI_IDENTITY_MATRIX));
  out[0] = scale_x;
  out[5] = scale_y;
  out[12] = trans_x;
  out[13] = trans_y;
}

static void CTUI_multiplyMatrix4x4(float *out, const float *a, const float *b) {
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      out[col * 4 + row] = 0.0f;
      for (int k = 0; k < 4; k++) {
        out[col * 4 + row] += a[k * 4 + row] * b[col * 4 + k];
      }
    }
  }
}

// GLFW Console structure
typedef struct CTUI_GlfwConsole {
  CTUI_Console base;
  GLFWwindow *window;
  CTUI_Renderer *renderer;
  CTUI_DVector2 tile_pixel_wh;
  int is_fullscreen;
  int is_visible;
  float viewport_translation[2];
  float viewport_scale[2];
  float base_transform[16];
} CTUI_GlfwConsole;

static size_t CTUI_GLFW_CONSOLE_COUNT = 0;

// Forward declarations
static void CTUI_updateBaseTransform(CTUI_GlfwConsole *glfw_console);
static void CTUI_getCombinedTransform(CTUI_GlfwConsole *glfw_console, float *out);

// Platform callbacks implementation

static void CTUI_destroyGlfwConsole(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  
  if (glfw_console->renderer) {
    glfwMakeContextCurrent(glfw_console->window);
    CTUI_rendererDestroy(glfw_console->renderer);
    glfw_console->renderer = NULL;
  }
  
  if (console->_layers) {
    for (size_t i = 0; i < console->_layer_count; i++) {
      if (console->_layers[i]._tiles)
        free(console->_layers[i]._tiles);
    }
    free(console->_layers);
    console->_layers = NULL;
  }
  
  if (glfw_console->window) {
    glfwDestroyWindow(glfw_console->window);
    glfw_console->window = NULL;
  }
  
  free(glfw_console);
  
  CTUI_GLFW_CONSOLE_COUNT--;
  if (CTUI_GLFW_CONSOLE_COUNT == 0) {
    glfwTerminate();
  }
}

static void CTUI_refreshGlfwConsole(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  
  if (!glfw_console->is_visible || glfw_console->window == NULL) {
    return;
  }
  if (console->_console_tile_wh.x == 0 || console->_console_tile_wh.y == 0) {
    return;
  }
  
  glfwMakeContextCurrent(glfw_console->window);
  
  // Update transform and render
  float combined_transform[16];
  CTUI_getCombinedTransform(glfw_console, combined_transform);
  
  if (glfw_console->renderer) {
    glfw_console->renderer->vtable->setTransform(glfw_console->renderer, combined_transform);
    glfw_console->renderer->vtable->render(glfw_console->renderer, console);
  }
  
  glfwSwapBuffers(glfw_console->window);
}

static void CTUI_glfwKeyCallback(GLFWwindow *window, int key, int scancode,
                                 int action, int mods) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL) return;
  
  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_KEY;
  ev.console = console;
  ev.data.key.key = (CTUI_Key)key;
  ev.data.key.scancode = scancode;
  // NOTE: REPEAT is treated as PRESS for compatibility with ncurses
  if (action == GLFW_PRESS || action == GLFW_REPEAT)
    ev.data.key.action = CTUIA_PRESS;
  else
    ev.data.key.action = CTUIA_RELEASE;
  ev.data.key.mods = mods;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwMouseButtonCallback(GLFWwindow *window, int button,
                                         int action, int mods) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL) return;
  
  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_MOUSE_BUTTON;
  ev.console = console;
  ev.data.mouse_button.button = button;
  ev.data.mouse_button.action = (action == GLFW_PRESS) ? CTUIA_PRESS : CTUIA_RELEASE;
  ev.data.mouse_button.mods = mods;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwCursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL) return;
  
  CTUI_Console *console = &glfw_console->base;
  double tile_x = xpos / glfw_console->tile_pixel_wh.x;
  double tile_y = ypos / glfw_console->tile_pixel_wh.y;
  
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_CURSOR_POS;
  ev.console = console;
  ev.data.cursor_pos.viewport_xy.x = xpos;
  ev.data.cursor_pos.viewport_xy.y = ypos;
  ev.data.cursor_pos.tile_xy.x = tile_x;
  ev.data.cursor_pos.tile_xy.y = tile_y;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL) return;
  
  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_SCROLL;
  ev.console = console;
  ev.data.scroll.scroll_xy.x = xoffset;
  ev.data.scroll.scroll_xy.y = yoffset;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwFramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL) return;
  
  glfwMakeContextCurrent(window);
  
  if (glfw_console->renderer) {
    glfw_console->renderer->vtable->resize(glfw_console->renderer, width, height);
  }
  
  CTUI_updateBaseTransform(glfw_console);
  
  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_RESIZE;
  ev.console = console;
  ev.data.resize.console_tile_wh = console->_console_tile_wh;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_pollEventsGlfwConsole(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  if (!glfw_console->window) return;
  
  glfwPollEvents();
  
  if (glfwWindowShouldClose(glfw_console->window)) {
    CTUI_Event ev = {0};
    ev.type = CTUI_EVENT_CLOSE;
    ev.console = console;
    CTUI_pushEvent(console->_ctx, &ev);
    glfwSetWindowShouldClose(glfw_console->window, GLFW_FALSE);
  }
}

static void CTUI_updateBaseTransform(CTUI_GlfwConsole *glfw_console) {
  CTUI_Console *console = &glfw_console->base;
  int win_w, win_h;
  glfwGetFramebufferSize(glfw_console->window, &win_w, &win_h);
  
  double grid_pixel_w = (double)console->_console_tile_wh.x * glfw_console->tile_pixel_wh.x;
  double grid_pixel_h = (double)console->_console_tile_wh.y * glfw_console->tile_pixel_wh.y;
  
  float scale_x = (float)(grid_pixel_w / (double)win_w);
  float scale_y = (float)(grid_pixel_h / (double)win_h);
  float offset_x = -1.0f + scale_x;
  float offset_y = 1.0f - scale_y;
  
  memcpy(glfw_console->base_transform, CTUI_IDENTITY_MATRIX, sizeof(CTUI_IDENTITY_MATRIX));
  glfw_console->base_transform[0] = scale_x;
  glfw_console->base_transform[5] = scale_y;
  glfw_console->base_transform[12] = offset_x;
  glfw_console->base_transform[13] = offset_y;
}

static void CTUI_getCombinedTransform(CTUI_GlfwConsole *glfw_console, float *out) {
  float viewport[16];
  CTUI_createTransformMatrix(viewport,
                              glfw_console->viewport_translation[0],
                              glfw_console->viewport_translation[1],
                              glfw_console->viewport_scale[0],
                              glfw_console->viewport_scale[1]);
  CTUI_multiplyMatrix4x4(out, viewport, glfw_console->base_transform);
}

static void CTUI_transformViewportGlfw(CTUI_Console *console,
                                       CTUI_FVector2 translation,
                                       CTUI_FVector2 scale) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfw_console->viewport_translation[0] = translation.x;
  glfw_console->viewport_translation[1] = translation.y;
  glfw_console->viewport_scale[0] = scale.x;
  glfw_console->viewport_scale[1] = scale.y;
}

static void CTUI_resetViewportGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfw_console->viewport_translation[0] = 0.0f;
  glfw_console->viewport_translation[1] = 0.0f;
  glfw_console->viewport_scale[0] = 1.0f;
  glfw_console->viewport_scale[1] = 1.0f;
}

static CTUI_DVector2 CTUI_getCursorViewportPosGlfw(CTUI_Console *console) {
  CTUI_DVector2 result = {0, 0};
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwGetCursorPos(glfw_console->window, &result.x, &result.y);
  return result;
}

static CTUI_DVector2 CTUI_getCursorTilePosGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  double px, py;
  glfwGetCursorPos(glfw_console->window, &px, &py);
  
  double tile_x = px / (double)glfw_console->tile_pixel_wh.x;
  double tile_y = py / (double)glfw_console->tile_pixel_wh.y;
  
  double console_w = (double)console->_console_tile_wh.x;
  double console_h = (double)console->_console_tile_wh.y;
  
  double trans_tiles_x = glfw_console->viewport_translation[0] * console_w / 2.0;
  double trans_tiles_y = -glfw_console->viewport_translation[1] * console_h / 2.0;
  
  CTUI_DVector2 result;
  result.x = (tile_x - trans_tiles_x) / (double)glfw_console->viewport_scale[0];
  result.y = (tile_y - trans_tiles_y) / (double)glfw_console->viewport_scale[1];
  return result;
}

static int CTUI_getMouseButtonGlfw(CTUI_Console *console, int button) {
  if (button < 0 || button > 7) return 0;
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetMouseButton(glfw_console->window, button) == GLFW_PRESS;
}

static int CTUI_getKeyStateGlfw(CTUI_Console *console, int key) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetKey(glfw_console->window, key) == GLFW_PRESS;
}

static void CTUI_setWindowPixelWhGlfw(CTUI_Console *console, CTUI_IVector2 pixel_wh) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwSetWindowSize(glfw_console->window, pixel_wh.x, pixel_wh.y);
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
}

static CTUI_IVector2 CTUI_getWindowPixelWhGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  CTUI_IVector2 result;
  glfwGetWindowSize(glfw_console->window, &result.x, &result.y);
  return result;
}

static void CTUI_setViewportTileWhGlfw(CTUI_Console *console, CTUI_SVector2 tile_wh) {
  console->_console_tile_wh = tile_wh;
}

static void CTUI_fitWindowPixelWhToViewportTileWhGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  int win_w = (int)((double)console->_console_tile_wh.x * glfw_console->tile_pixel_wh.x);
  int win_h = (int)((double)console->_console_tile_wh.y * glfw_console->tile_pixel_wh.y);
  glfwSetWindowSize(glfw_console->window, win_w, win_h);
  
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
  
  glfwMakeContextCurrent(glfw_console->window);
  int fb_w, fb_h;
  glfwGetFramebufferSize(glfw_console->window, &fb_w, &fb_h);
  
  if (glfw_console->renderer) {
    glfw_console->renderer->vtable->resize(glfw_console->renderer, fb_w, fb_h);
  }
  
  CTUI_updateBaseTransform(glfw_console);
}

static void CTUI_fitViewportTileWhToWindowPixelWhGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  int win_w, win_h;
  glfwGetWindowSize(glfw_console->window, &win_w, &win_h);
  
  size_t new_tiles_x = (size_t)((double)win_w / glfw_console->tile_pixel_wh.x);
  size_t new_tiles_y = (size_t)((double)win_h / glfw_console->tile_pixel_wh.y);
  if (new_tiles_x < 1) new_tiles_x = 1;
  if (new_tiles_y < 1) new_tiles_y = 1;
  
  if (console->_console_tile_wh.x != new_tiles_x ||
      console->_console_tile_wh.y != new_tiles_y) {
    console->_console_tile_wh.x = new_tiles_x;
    console->_console_tile_wh.y = new_tiles_y;
    CTUI_updateBaseTransform(glfw_console);
  }
}

static void CTUI_setWindowResizableGlfw(CTUI_Console *console, int resizable) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_RESIZABLE,
                      resizable ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowResizableGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_RESIZABLE) == GLFW_TRUE;
}

static int CTUI_getIsFullscreenGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfw_console->is_fullscreen;
}

static void CTUI_setWindowDecoratedGlfw(CTUI_Console *console, int decorated) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_DECORATED,
                      decorated ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowDecoratedGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_DECORATED) == GLFW_TRUE;
}

static void CTUI_setWindowFloatingGlfw(CTUI_Console *console, int floating) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_FLOATING,
                      floating ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowFloatingGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_FLOATING) == GLFW_TRUE;
}

static void CTUI_minimizeWindowGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwIconifyWindow(glfw_console->window);
}

static void CTUI_maximizeWindowGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwMaximizeWindow(glfw_console->window);
}

static void CTUI_restoreWindowGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwRestoreWindow(glfw_console->window);
}

static int CTUI_getWindowMinimizedGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_ICONIFIED) == GLFW_TRUE;
}

static int CTUI_getWindowMaximizedGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_MAXIMIZED) == GLFW_TRUE;
}

static void CTUI_focusWindowGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwFocusWindow(glfw_console->window);
}

static int CTUI_getWindowFocusedGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_FOCUSED) == GLFW_TRUE;
}

static void CTUI_requestWindowAttentionGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwRequestWindowAttention(glfw_console->window);
}

static void CTUI_setWindowOpacityGlfw(CTUI_Console *console, float opacity) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  glfwSetWindowOpacity(glfw_console->window, opacity);
}

static float CTUI_getWindowOpacityGlfw(CTUI_Console *console) {
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  return glfwGetWindowOpacity(glfw_console->window);
}

// Forward declarations for window management functions defined after vtable
static void CTUI_hideWindowGlfw(CTUI_Console *console);
static void CTUI_showWindowGlfw(CTUI_Console *console);
static void CTUI_setWindowedTileWhGlfw(CTUI_Console *console, CTUI_SVector2 tile_wh);
static void CTUI_setWindowedFullscreenGlfw(CTUI_Console *console);

// Platform vtable
static CTUI_PlatformVtable CTUI_PLATFORM_VTABLE_GLFW = {
    .is_resizable = 1,
    .destroy = CTUI_destroyGlfwConsole,
    .resize = NULL,
    .refresh = CTUI_refreshGlfwConsole,
    .pollEvents = CTUI_pollEventsGlfwConsole,
    .getCursorViewportPos = CTUI_getCursorViewportPosGlfw,
    .getCursorTilePos = CTUI_getCursorTilePosGlfw,
    .getMouseButton = CTUI_getMouseButtonGlfw,
    .getKeyState = CTUI_getKeyStateGlfw,
    .transformViewport = CTUI_transformViewportGlfw,
    .resetViewport = CTUI_resetViewportGlfw,
    .setWindowPixelWh = CTUI_setWindowPixelWhGlfw,
    .getWindowPixelWh = CTUI_getWindowPixelWhGlfw,
    .setViewportTileWh = CTUI_setViewportTileWhGlfw,
    .fitWindowPixelWhToViewportTileWh = CTUI_fitWindowPixelWhToViewportTileWhGlfw,
    .fitViewportTileWhToWindowPixelWh = CTUI_fitViewportTileWhToWindowPixelWhGlfw,
    .setWindowResizable = CTUI_setWindowResizableGlfw,
    .getWindowResizable = CTUI_getWindowResizableGlfw,
    .getIsFullscreen = CTUI_getIsFullscreenGlfw,
    .setWindowDecorated = CTUI_setWindowDecoratedGlfw,
    .getWindowDecorated = CTUI_getWindowDecoratedGlfw,
    .setWindowFloating = CTUI_setWindowFloatingGlfw,
    .getWindowFloating = CTUI_getWindowFloatingGlfw,
    .minimizeWindow = CTUI_minimizeWindowGlfw,
    .maximizeWindow = CTUI_maximizeWindowGlfw,
    .restoreWindow = CTUI_restoreWindowGlfw,
    .getWindowMinimized = CTUI_getWindowMinimizedGlfw,
    .getWindowMaximized = CTUI_getWindowMaximizedGlfw,
    .focusWindow = CTUI_focusWindowGlfw,
    .getWindowFocused = CTUI_getWindowFocusedGlfw,
    .requestWindowAttention = CTUI_requestWindowAttentionGlfw,
    .setWindowOpacity = CTUI_setWindowOpacityGlfw,
    .getWindowOpacity = CTUI_getWindowOpacityGlfw,
    .hideWindow = CTUI_hideWindowGlfw,
    .showWindow = CTUI_showWindowGlfw,
    .setWindowedTileWh = CTUI_setWindowedTileWhGlfw,
    .setWindowedFullscreen = CTUI_setWindowedFullscreenGlfw
};

static CTUI_Console *CTUI_createGlfwConsoleFromWindow(
    CTUI_Context *ctx, void *glfw_window, CTUI_Renderer *renderer, 
    CTUI_DVector2 tile_pixel_wh, size_t layer_count, 
    const CTUI_LayerInfo *layer_infos, CTUI_ColorMode color_mode) {
  
  GLFWwindow *window = (GLFWwindow *)glfw_window;
  if (window == NULL) {
    return NULL;
  }
  
  CTUI_GlfwConsole *glfw_console = calloc(1, sizeof(CTUI_GlfwConsole));
  if (glfw_console == NULL) {
    glfwDestroyWindow(window);
    return NULL;
  }
  
  CTUI_GLFW_CONSOLE_COUNT++;
  glfw_console->window = window;
  glfw_console->tile_pixel_wh = tile_pixel_wh;
  glfw_console->is_fullscreen = 0;
  glfw_console->is_visible = 0;
  glfw_console->viewport_translation[0] = 0.0f;
  glfw_console->viewport_translation[1] = 0.0f;
  glfw_console->viewport_scale[0] = 1.0f;
  glfw_console->viewport_scale[1] = 1.0f;
  memcpy(glfw_console->base_transform, CTUI_IDENTITY_MATRIX, sizeof(CTUI_IDENTITY_MATRIX));
  
  glfwSetWindowUserPointer(window, glfw_console);
  glfwSetKeyCallback(window, CTUI_glfwKeyCallback);
  glfwSetMouseButtonCallback(window, CTUI_glfwMouseButtonCallback);
  glfwSetCursorPosCallback(window, CTUI_glfwCursorPosCallback);
  glfwSetScrollCallback(window, CTUI_glfwScrollCallback);
  glfwSetFramebufferSizeCallback(window, CTUI_glfwFramebufferSizeCallback);
  glfwMakeContextCurrent(window);
  glfw_console->renderer = renderer;
    if (glfw_console->renderer != NULL) {
    if (CTUI_rendererInit(glfw_console->renderer) != 0) {
      CTUI_destroyGlfwConsole(&glfw_console->base);
      return NULL;
    }
  }
  
  // Initialize console base
  CTUI_Console *console = &glfw_console->base;
  console->_platform = &CTUI_PLATFORM_VTABLE_GLFW;
  console->_ctx = ctx;
  console->_is_real_terminal = 0;
  console->_effective_color_mode = color_mode;
  console->_console_tile_wh = (CTUI_SVector2){.x = 0, .y = 0};
  console->_layer_count = layer_count;
  console->_layers = calloc(layer_count, sizeof(CTUI_ConsoleLayer));
  
  if (console->_layers == NULL) {
    CTUI_destroyGlfwConsole(console);
    return NULL;
  }
  
  for (size_t i = 0; i < layer_count; ++i) {
    console->_layers[i]._tile_div_wh = layer_infos[i].tile_div_wh;
    if (console->_layers[i]._tile_div_wh.x == 0)
      console->_layers[i]._tile_div_wh.x = 1.0;
    if (console->_layers[i]._tile_div_wh.y == 0)
      console->_layers[i]._tile_div_wh.y = 1.0;
    console->_layers[i]._font = layer_infos[i].font;
    console->_layers[i]._tiles = NULL;
    console->_layers[i]._tiles_count = 0;
    console->_layers[i]._tiles_capacity = 0;
  }
  
  // Link to context
  if (ctx->_first_console != NULL) {
    ctx->_first_console->_prev = console;
  }
  console->_next = ctx->_first_console;
  ctx->_first_console = console;
  
  return console;
}

static void CTUI_hideWindowGlfw(CTUI_Console *console) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  if (glfw_console->window) {
    glfwHideWindow(glfw_console->window);
    glfw_console->is_visible = 0;
  }
}

static void CTUI_showWindowGlfw(CTUI_Console *console) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  if (glfw_console->window && !glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
}

static void CTUI_setWindowedTileWhGlfw(CTUI_Console *console, CTUI_SVector2 console_tile_wh) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  
  if (glfw_console->is_fullscreen) {
    glfwSetWindowMonitor(
        glfw_console->window, NULL, 100, 100,
        (int)((double)console_tile_wh.x * glfw_console->tile_pixel_wh.x),
        (int)((double)console_tile_wh.y * glfw_console->tile_pixel_wh.y), 0);
    glfw_console->is_fullscreen = 0;
  } else {
    int win_w = (int)((double)console_tile_wh.x * glfw_console->tile_pixel_wh.x);
    int win_h = (int)((double)console_tile_wh.y * glfw_console->tile_pixel_wh.y);
    glfwSetWindowSize(glfw_console->window, win_w, win_h);
  }
  
  console->_console_tile_wh = console_tile_wh;
  
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
  
  glfwMakeContextCurrent(glfw_console->window);
  int fb_w, fb_h;
  glfwGetFramebufferSize(glfw_console->window, &fb_w, &fb_h);
  
  if (glfw_console->renderer) {
    glfw_console->renderer->vtable->resize(glfw_console->renderer, fb_w, fb_h);
  }
  
  CTUI_updateBaseTransform(glfw_console);
  CTUI_clear(console);
}

static void CTUI_setWindowedFullscreenGlfw(CTUI_Console *console) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwConsole *glfw_console = (CTUI_GlfwConsole *)console;
  
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = glfwGetVideoMode(monitor);
  
  size_t tiles_x = (size_t)((double)mode->width / glfw_console->tile_pixel_wh.x);
  size_t tiles_y = (size_t)((double)mode->height / glfw_console->tile_pixel_wh.y);
  
  console->_console_tile_wh.x = tiles_x;
  console->_console_tile_wh.y = tiles_y;
  
  glfwSetWindowMonitor(glfw_console->window, monitor, 0, 0, mode->width,
                       mode->height, mode->refreshRate);
  glfw_console->is_fullscreen = 1;
  
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
  
  glfwMakeContextCurrent(glfw_console->window);
  int fb_w, fb_h;
  glfwGetFramebufferSize(glfw_console->window, &fb_w, &fb_h);
  
  if (glfw_console->renderer) {
    glfw_console->renderer->vtable->resize(glfw_console->renderer, fb_w, fb_h);
  }
  
  CTUI_updateBaseTransform(glfw_console);
  CTUI_clear(console);
}

CTUI_Console *CTUI_createGlfwOpengl33FakeTerminal(
    CTUI_Context *context, CTUI_DVector2 tile_pixel_wh,
    size_t layer_count, const CTUI_LayerInfo *layer_infos,
    CTUI_ColorMode color_mode, const char *title) {
  if (glfwInit() == GLFW_FALSE) {
    return NULL;
  }
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  GLFWwindow *window = glfwCreateWindow(640, 480, title ? title : "CTUI Console", NULL, NULL);
  if (window == NULL) {
    return NULL;
  }
  glfwMakeContextCurrent(window);
  CTUI_Renderer *renderer = CTUI_createOpenGL33Renderer(
      (CTUI_GLGetProcAddress)glfwGetProcAddress);
  if (renderer == NULL) {
    glfwDestroyWindow(window);
    return NULL;
  }
  CTUI_Console *console = CTUI_createGlfwConsoleFromWindow(
      context, window, renderer, tile_pixel_wh, layer_count, layer_infos,
      color_mode);
  if (console == NULL) {
    CTUI_destroyOpenGL33Renderer(renderer);
    glfwDestroyWindow(window);
    return NULL;
  }
  
  return console;
}