#include "frontend_common.h"
#include <ctui/ctui.h>
#include <stdlib.h>
#include <time.h>

typedef struct Trail {
  int alive;
  int x;
  int head;
  int length;
} Trail;

static size_t TRAIL_COUNT;
static size_t TRAIL_CAPACITY;
static Trail *TRAILS;

int startApp(CTUI_Context *ctx, CTUI_Console *console) {
  const CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
  TRAIL_CAPACITY = console_tile_wh.x;
  TRAILS = (Trail *)calloc(TRAIL_CAPACITY, sizeof(Trail));
  if (TRAILS == NULL) {
    return 0;
  }
  TRAIL_COUNT = TRAIL_CAPACITY;
  srand(time(NULL));
  return 1;
}

void cleanup(CTUI_Context *ctx, CTUI_Console *console) {
  if (TRAILS != NULL) {
    free(TRAILS);
  }
}

int runMainLoopBody(CTUI_Context *ctx, CTUI_Console *console) {
  CTUI_pollEvents(ctx);
  CTUI_Event event;
  while (CTUI_nextEvent(ctx, &event)) {
    switch (event.type) {
    case CTUI_EVENT_CLOSE:
      return 0;
    case CTUI_EVENT_KEY:
      if (event.data.key.key == CTUIK_ESCAPE &&
          event.data.key.action == CTUIA_PRESS) {
        return 0;
      }
      break;
    case CTUI_EVENT_RESIZE: {
      CTUI_fitViewportTileWhToWindowPixelWh(console);
      const CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
      TRAIL_COUNT = console_tile_wh.x;
      if (TRAIL_COUNT >= TRAIL_CAPACITY) {
        Trail *new_trails = (Trail *)realloc(TRAILS, TRAIL_COUNT * sizeof(Trail));
        if (new_trails == NULL) {
          return 0;
        }
        TRAILS = new_trails;
        if (TRAIL_COUNT > TRAIL_CAPACITY) {
          for (size_t i = TRAIL_CAPACITY; i < TRAIL_COUNT; i++) {
            TRAILS[i] = (Trail){0};
          }
        }
        TRAIL_CAPACITY = TRAIL_COUNT;
      }
    } break;
    default:
      break;
    }
  }
  const CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
  const int screen_height = console_tile_wh.y;
  const int screen_width = console_tile_wh.x;
  CTUI_clear(console);
  for (size_t i = 0; i < TRAIL_COUNT && i < screen_width; i++) {
    Trail *trail = &TRAILS[i];
    if (!trail->alive) {
      if (rand() % 50 == 0) {
        trail->alive = 1;
        trail->x = i;
        trail->head = 0;
        trail->length = 5 + rand() % 15; // 5-19
      }
    } else {
      trail->head++;
      if (trail->head - trail->length >= screen_height) {
        trail->alive = 0;
      }
    }
  }
  CTUI_ConsoleLayer *layer = CTUI_getConsoleLayer(console, 0);
  if (layer == NULL) {
    CTUI_refresh(console);
    return 1;
  }
  CTUI_Color bright_green = {
      .palette8 = 2,                         // Green
      .palette16 = 10,                       // Bright green
      .palette256 = 46,                      // Bright green
      .full = (CTUI_Color32){0, 255, 0, 255} // Bright green RGB
  };
  CTUI_Color dark_green = {
      .palette8 = 2,    // Green (same as bright in 8-color mode)
      .palette16 = 2,   // Dark green
      .palette256 = 22, // Dark green
      .full = (CTUI_Color32){0, 128, 0, 255} // Dark green RGB
  };
  CTUI_Color black = {
      .palette8 = 0,                       // Black
      .palette16 = 0,                      // Black
      .palette256 = 0,                     // Black
      .full = (CTUI_Color32){0, 0, 0, 255} // Black RGB
  };
  for (size_t i = 0; i < TRAIL_COUNT && i < screen_width; i++) {
    Trail *trail = &TRAILS[i];
    if (trail->alive == 0) {
      continue;
    }
    for (int j = 0; j < trail->length; j++) {
      int y = trail->head - j;
      if (y >= 0 && y < screen_height) {
        char digit = '0' + (rand() % 10);
        CTUI_Color fg_color;
        if (j == 0) {
          fg_color = bright_green;
        } else {
          float intensity = 1.0f - (float)j / trail->length;
          if (intensity > 0.4f) {
            fg_color = bright_green;
          } else {
            fg_color = dark_green;
          }
        }
        CTUI_pushCodepoint(layer, (uint32_t)digit, (CTUI_IVector2){trail->x, y},
                           fg_color, black);
      }
    }
  }
  CTUI_refresh(console);
  return 1;
}