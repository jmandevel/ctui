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

static int ANIMATION_RUNNING = 1;

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
      } else if (event.data.key.key == CTUIK_SPACE &&
                 event.data.key.action == CTUIA_PRESS) {
        ANIMATION_RUNNING = !ANIMATION_RUNNING;
      } else if (event.data.key.key == CTUIK_F &&
                 event.data.key.action == CTUIA_PRESS) {
        if (CTUI_getIsFullscreen(console)) {
          CTUI_setWindowedTileWh(console, (CTUI_SVector2){100, 100});
        } else {
          CTUI_setWindowedFullscreen(console);
        }
      }
      break;
    case CTUI_EVENT_RESIZE: {
      CTUI_fitViewportTileWhToWindowPixelWh(console);
      const CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
      const size_t new_trail_count = console_tile_wh.x;
      if (new_trail_count > TRAIL_CAPACITY) {
        Trail *new_trails =
            (Trail *)realloc(TRAILS, new_trail_count * sizeof(Trail));
        if (new_trails == NULL) {
          return 0;
        }
        TRAILS = new_trails;
        TRAIL_CAPACITY = new_trail_count;
        for (size_t i = TRAIL_COUNT; i < new_trail_count; i++) {
          TRAILS[i] = (Trail){0};
        }
      } else if (new_trail_count < TRAIL_CAPACITY) {
        for (size_t i = TRAIL_COUNT; i < TRAIL_CAPACITY; i++) {
          TRAILS[i] = (Trail){0};
        }
      }
      TRAIL_COUNT = new_trail_count;
    } break;
    default:
      break;
    }
  }
  const CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
  const int screen_height = console_tile_wh.y;
  const int screen_width = console_tile_wh.x;
  if (ANIMATION_RUNNING) {
    for (size_t i = 0; i < TRAIL_COUNT; i++) {
      Trail *trail = &TRAILS[i];
      if (!trail->alive) {
        if (rand() % 50 == 0) {
          trail->alive = 1;
          trail->x = i;
          trail->head = 0;
          trail->length = 5 + rand() % 50;
        }
      } else {
        trail->head++;
        if (trail->head - trail->length >= screen_height) {
          trail->alive = 0;
        }
      }
    }
  }
  CTUI_ConsoleLayer *layer0 = CTUI_getConsoleLayer(console, 0);
  CTUI_ConsoleLayer *layer1 = CTUI_getConsoleLayer(console, 1);
  CTUI_pushCstr(layer1, "Hello, and welcome to CTUI!", (CTUI_IVector2){1, 1},
                99, 0, CTUI_BRIGHT_RED, CTUI_BLACK);
  CTUI_pushCstr(layer1, "Press spacebar to start and stop time.",
                (CTUI_IVector2){1, 4}, 99, 0, CTUI_BRIGHT_RED, CTUI_BLACK);
  CTUI_pushCstr(layer1, "Press f to toggle fullscreen.", (CTUI_IVector2){1, 6},
                99, 0, CTUI_BRIGHT_RED, CTUI_BLACK);
  CTUI_pushCstr(layer1,
                "Notice how this text uses a seperate font that is half width.",
                (CTUI_IVector2){1, 9}, 99, 0, CTUI_BRIGHT_RED, CTUI_BLACK);
  CTUI_fill(layer0, ' ', CTUI_BLACK, CTUI_BLACK);
  for (size_t i = 0; i < TRAIL_COUNT; i++) {
    Trail *trail = &TRAILS[i];
    if (trail->alive == 0) {
      continue;
    }
    for (int j = 0; j < trail->length; j++) {
      int y = trail->head - j;
      if (y >= 0 && y < screen_height) {
        char digit = '0' + (rand() % 10);
        CTUI_Color fg_color;
        float intensity = 1.0f - (float)j / trail->length;
        fg_color = CTUI_RGBA32N(0, intensity, 0, 1);
        CTUI_pushCodepoint(layer0, (uint32_t)digit,
                           (CTUI_IVector2){trail->x, y}, fg_color, CTUI_BLACK);
      }
    }
  }
  CTUI_refresh(ctx);
  return 1;
}