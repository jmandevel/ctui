#ifndef FRONTEND_COMMON_H
#define FRONTEND_COMMON_H

#include <ctui/ctui.h>

int startApp(CTUI_Context* ctx, CTUI_Console* console);

int runMainLoopBody(CTUI_Context* ctx, CTUI_Console* console);

void cleanup(CTUI_Context* ctx, CTUI_Console* console);

#endif