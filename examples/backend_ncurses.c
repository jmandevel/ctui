#include <ctui/ctui.h>
#include "frontend_common.h"

int main() {
    if (!CTUI_getHasRealTerminal()) {
        return 1;
    }
    CTUI_Context* ctx = CTUI_createContext();
    if (ctx == NULL) {
        return 2;
    }
    const size_t layer_count = 2;
    CTUI_Console* console = CTUI_createNcursesRealTerminal(ctx, layer_count, CTUI_COLORMODE_ANSI256, CTUI_COLORMODE_NO_COLORS);

    if (!startApp(ctx, console)) {
        return 3;
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