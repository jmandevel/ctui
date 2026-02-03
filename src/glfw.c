#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctui/ctui.h>

static const CTUI_Matrix4x4 CTUI_IDENTITY_MATRIX = {
    .m = {1.0f, 0.0f, 0.0f, 0.0f,
          0.0f, 1.0f, 0.0f, 0.0f,
          0.0f, 0.0f, 1.0f, 0.0f,
          0.0f, 0.0f, 0.0f, 1.0f}};

static CTUI_Matrix4x4 CTUI_createTransformMatrix(CTUI_FVector2 translation,
                                               CTUI_FVector2 scale) {
  CTUI_Matrix4x4 result = CTUI_IDENTITY_MATRIX;
  result.m[0] = scale.x;
  result.m[5] = scale.y;
  result.m[12] = translation.x;
  result.m[13] = translation.y;
  return result;
}

typedef struct CTUI_GlVertex {
  float x, y;
  float u, v, page;
  float fg[4];
  float bg[4];
} CTUI_GlVertex;

typedef struct CTUI_GlfwGlBuffer {
  GLuint vbo;
  size_t vertex_count;
  size_t vertex_capacity;
  CTUI_GlVertex *vertex_data;
} CTUI_GlfwGlBuffer;

typedef struct CTUI_FontTexture {
  CTUI_Font *font;
  GLuint texture;
} CTUI_FontTexture;

typedef struct CTUI_GlfwGlConsole {
  CTUI_Console base;
  GLFWwindow *window;
  CTUI_DVector2 tile_pixel_wh;
  int is_fullscreen;
  int is_visible;
  CTUI_FVector2 viewport_translation;
  CTUI_FVector2 viewport_scale;
  CTUI_Matrix4x4 base_transform;
  GLuint shader;
  GLint transform_uniform_loc;
  GLuint vao;
  size_t buffer_count;
  CTUI_GlfwGlBuffer *buffers;
  size_t font_texture_count;
  CTUI_FontTexture *font_textures;
} CTUI_GlfwGlConsole;

static const char *VERTEX_SHADER_SRC =
    "#version 330 core\n"
    "layout(location = 0) in vec2 in_pos;\n"
    "layout(location = 1) in vec3 in_uvp;\n"
    "layout(location = 2) in vec4 in_fg;\n"
    "layout(location = 3) in vec4 in_bg;\n"
    "uniform mat4 u_transform;\n"
    "out vec3 uvp;\n"
    "out vec4 fg;\n"
    "out vec4 bg;\n"
    "void main() {\n"
    "    gl_Position = u_transform * vec4(in_pos, 0.0, 1.0);\n"
    "    uvp = in_uvp;\n"
    "    fg = in_fg;\n"
    "    bg = in_bg;\n"
    "}\n";

static const char *FRAGMENT_SHADER_SRC =
    "#version 330 core\n"
    "uniform sampler2DArray tex;\n"
    "in vec3 uvp;\n"
    "in vec4 fg;\n"
    "in vec4 bg;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    vec4 texel = texture(tex, uvp);\n"
    "    out_color = mix(bg, fg, texel.a);\n"
    "}\n";

static GLuint CTUI_compileGlShader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  // TODO check compile errors
  return shader;
}

static GLuint CTUI_createGlProgram(GLint *out_transform_loc) {
  GLuint vs = CTUI_compileGlShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
  GLuint fs = CTUI_compileGlShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  // TODO check link errors
  glDeleteShader(vs);
  glDeleteShader(fs);
  *out_transform_loc = glGetUniformLocation(prog, "u_transform");
  return prog;
}

static GLuint CTUI_createFontTexture2DArray(CTUI_Font *font) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, font->_image._width,
               font->_image._height, font->_image._pages, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, font->_image._pixels);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  return texture;
}

static GLuint CTUI_getOrCreateFontTexture(CTUI_GlfwGlConsole *glfw_console,
                                          CTUI_Font *font) {
  for (size_t i = 0; i < glfw_console->font_texture_count; i++) {
    if (glfw_console->font_textures[i].font == font) {
      return glfw_console->font_textures[i].texture; // this console already has a texture for this font...
    }
  }
  GLuint texture = CTUI_createFontTexture2DArray(font);
  size_t new_count = glfw_console->font_texture_count + 1;
  CTUI_FontTexture *new_textures =
      realloc(glfw_console->font_textures, sizeof(CTUI_FontTexture) * new_count);
  if (new_textures == NULL) {
    // TODO
    glDeleteTextures(1, &texture);
    return 0;
  }
  glfw_console->font_textures = new_textures;
  glfw_console->font_textures[glfw_console->font_texture_count].font = font;
  glfw_console->font_textures[glfw_console->font_texture_count].texture = texture;
  glfw_console->font_texture_count = new_count;
  return texture;
}

static void CTUI_updateBaseTransform(CTUI_GlfwGlConsole *glfw_console) {
  CTUI_Console *console = &glfw_console->base;
  int win_w, win_h;
  glfwGetFramebufferSize(glfw_console->window, &win_w, &win_h);
  double grid_pixel_w = (double)console->_console_tile_wh.x * glfw_console->tile_pixel_wh.x;
  double grid_pixel_h = (double)console->_console_tile_wh.y * glfw_console->tile_pixel_wh.y;
  float scale_x = (float)(grid_pixel_w / (double)win_w);
  float scale_y = (float)(grid_pixel_h / (double)win_h);
  float offset_x = -1.0f + scale_x;
  float offset_y = 1.0f - scale_y;
  glfw_console->base_transform = CTUI_IDENTITY_MATRIX;
  glfw_console->base_transform.m[0] = scale_x;
  glfw_console->base_transform.m[5] = scale_y;
  glfw_console->base_transform.m[12] = offset_x;
  glfw_console->base_transform.m[13] = offset_y;
}

static CTUI_Matrix4x4 CTUI_getCombinedTransform(CTUI_GlfwGlConsole *glfw_console) {
  CTUI_Matrix4x4 viewport = CTUI_createTransformMatrix(
      glfw_console->viewport_translation, glfw_console->viewport_scale);
  CTUI_Matrix4x4 result;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      result.m[col * 4 + row] = 0.0f;
      for (int k = 0; k < 4; k++) {
        result.m[col * 4 + row] +=
            viewport.m[k * 4 + row] * glfw_console->base_transform.m[col * 4 + k];
      }
    }
  }
  return result;
}

static void CTUI_refreshGlfwGlConsole(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  if (!glfw_console->is_visible || glfw_console->window == NULL) {
    return;
  }
  if (console->_console_tile_wh.x == 0 || console->_console_tile_wh.y == 0) {
    return;
  }
  glfwMakeContextCurrent(glfw_console->window);
  for (size_t buffer_i = 0; buffer_i < glfw_console->buffer_count; buffer_i++) {
    CTUI_GlfwGlBuffer *buffer = &glfw_console->buffers[buffer_i];
    buffer->vertex_count = 0;
    CTUI_ConsoleLayer *layer = &console->_layers[buffer_i];
    if (layer->_font == NULL) {
      continue;
    }
    if (layer->_tile_div_wh.x == 0 || layer->_tile_div_wh.y == 0) {
      continue;
    }
    CTUI_Font *font = layer->_font;
    CTUI_FVector2 tile_screen_wh = (CTUI_FVector2){
        .x = 2.0f / (float)((double)console->_console_tile_wh.x * layer->_tile_div_wh.x),
        .y = 2.0f / (float)((double)console->_console_tile_wh.y * layer->_tile_div_wh.y)};
    if (buffer->vertex_capacity < layer->_tiles_count * 6) {
      CTUI_GlVertex *new_vertex_data =
          realloc(buffer->vertex_data, layer->_tiles_count * 6 * sizeof(CTUI_GlVertex));
      if (new_vertex_data == NULL) {
        // TODO
        continue;
      }
      buffer->vertex_data = new_vertex_data;
      buffer->vertex_capacity = layer->_tiles_count * 6;
    }
    for (size_t tile_i = 0; tile_i < layer->_tiles_count; tile_i++) {
      CTUI_ConsoleTile *tile = &layer->_tiles[tile_i];
      float left_x = ((float)tile->_pos_xy.x * tile_screen_wh.x) - 1.0f;
      float right_x = left_x + tile_screen_wh.x;
      float top_y = 1.0f - ((float)tile->_pos_xy.y * tile_screen_wh.y);
      float bottom_y = top_y - tile_screen_wh.y;
      CTUI_Glyph *glyph = CTUI_tryGetGlyph(font, tile->_codepoint);
      if (glyph == NULL) {
        // TODO: use fallback glyph
        continue;
      }
      float fg_r = (float)tile->_fg.full.r / 255.0f;
      float fg_g = (float)tile->_fg.full.g / 255.0f;
      float fg_b = (float)tile->_fg.full.b / 255.0f;
      float fg_a = (float)tile->_fg.full.a / 255.0f;
      float bg_r = (float)tile->_bg.full.r / 255.0f;
      float bg_g = (float)tile->_bg.full.g / 255.0f;
      float bg_b = (float)tile->_bg.full.b / 255.0f;
      float bg_a = (float)tile->_bg.full.a / 255.0f;
      CTUI_GlVertex *v0 = &buffer->vertex_data[buffer->vertex_count++];
      v0->x = left_x;
      v0->y = top_y;
      v0->u = glyph->_tex_coords.s;
      v0->v = glyph->_tex_coords.p;
      v0->page = glyph->_tex_coords.page;
      v0->fg[0] = fg_r; v0->fg[1] = fg_g; v0->fg[2] = fg_b; v0->fg[3] = fg_a;
      v0->bg[0] = bg_r; v0->bg[1] = bg_g; v0->bg[2] = bg_b; v0->bg[3] = bg_a;
      CTUI_GlVertex *v1 = &buffer->vertex_data[buffer->vertex_count++];
      v1->x = right_x;
      v1->y = top_y;
      v1->u = glyph->_tex_coords.t;
      v1->v = glyph->_tex_coords.p;
      v1->page = glyph->_tex_coords.page;
      v1->fg[0] = fg_r; v1->fg[1] = fg_g; v1->fg[2] = fg_b; v1->fg[3] = fg_a;
      v1->bg[0] = bg_r; v1->bg[1] = bg_g; v1->bg[2] = bg_b; v1->bg[3] = bg_a;
      CTUI_GlVertex *v2 = &buffer->vertex_data[buffer->vertex_count++];
      v2->x = left_x;
      v2->y = bottom_y;
      v2->u = glyph->_tex_coords.s;
      v2->v = glyph->_tex_coords.q;
      v2->page = glyph->_tex_coords.page;
      v2->fg[0] = fg_r; v2->fg[1] = fg_g; v2->fg[2] = fg_b; v2->fg[3] = fg_a;
      v2->bg[0] = bg_r; v2->bg[1] = bg_g; v2->bg[2] = bg_b; v2->bg[3] = bg_a;
      CTUI_GlVertex *v3 = &buffer->vertex_data[buffer->vertex_count++];
      v3->x = right_x;
      v3->y = top_y;
      v3->u = glyph->_tex_coords.t;
      v3->v = glyph->_tex_coords.p;
      v3->page = glyph->_tex_coords.page;
      v3->fg[0] = fg_r; v3->fg[1] = fg_g; v3->fg[2] = fg_b; v3->fg[3] = fg_a;
      v3->bg[0] = bg_r; v3->bg[1] = bg_g; v3->bg[2] = bg_b; v3->bg[3] = bg_a;
      CTUI_GlVertex *v4 = &buffer->vertex_data[buffer->vertex_count++];
      v4->x = right_x;
      v4->y = bottom_y;
      v4->u = glyph->_tex_coords.t;
      v4->v = glyph->_tex_coords.q;
      v4->page = glyph->_tex_coords.page;
      v4->fg[0] = fg_r; v4->fg[1] = fg_g; v4->fg[2] = fg_b; v4->fg[3] = fg_a;
      v4->bg[0] = bg_r; v4->bg[1] = bg_g; v4->bg[2] = bg_b; v4->bg[3] = bg_a;
      CTUI_GlVertex *v5 = &buffer->vertex_data[buffer->vertex_count++];
      v5->x = left_x;
      v5->y = bottom_y;
      v5->u = glyph->_tex_coords.s;
      v5->v = glyph->_tex_coords.q;
      v5->page = glyph->_tex_coords.page;
      v5->fg[0] = fg_r; v5->fg[1] = fg_g; v5->fg[2] = fg_b; v5->fg[3] = fg_a;
      v5->bg[0] = bg_r; v5->bg[1] = bg_g; v5->bg[2] = bg_b; v5->bg[3] = bg_a;
    }
  }
  if (console->_fill_bg_set) {
    float r = (float)console->_fill_bg_color.full.r / 255.0f;
    float g = (float)console->_fill_bg_color.full.g / 255.0f;
    float b = (float)console->_fill_bg_color.full.b / 255.0f;
    float a = (float)console->_fill_bg_color.full.a / 255.0f;
    glClearColor(r, g, b, a);
  } else {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  }
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(glfw_console->shader);
  CTUI_Matrix4x4 transform = CTUI_getCombinedTransform(glfw_console);
  glUniformMatrix4fv(glfw_console->transform_uniform_loc, 1, GL_FALSE, transform.m);
  glBindVertexArray(glfw_console->vao);
  for (size_t buffer_i = 0; buffer_i < glfw_console->buffer_count; buffer_i++) {
    CTUI_GlfwGlBuffer *buffer = &glfw_console->buffers[buffer_i];
    if (buffer->vertex_count == 0)
      continue;
    CTUI_ConsoleLayer *layer = &console->_layers[buffer_i];
    if (layer->_font == NULL)
      continue;
    CTUI_Font *font = layer->_font;
    GLuint texture = CTUI_getOrCreateFontTexture(glfw_console, font);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CTUI_GlVertex) * buffer->vertex_count,
                 buffer->vertex_data, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CTUI_GlVertex),
                          (void *)offsetof(CTUI_GlVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CTUI_GlVertex),
                          (void *)offsetof(CTUI_GlVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(CTUI_GlVertex),
                          (void *)offsetof(CTUI_GlVertex, fg));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(CTUI_GlVertex),
                          (void *)offsetof(CTUI_GlVertex, bg));

    glDrawArrays(GL_TRIANGLES, 0, buffer->vertex_count);
  }
  glfwSwapBuffers(glfw_console->window);
}

static size_t CTUI_GLFW_CONSOLE_COUNT = 0;

static void CTUI_destroyGlfwGlConsole(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwMakeContextCurrent(glfw_console->window);
  for (size_t i = 0; i < glfw_console->font_texture_count; i++) {
    if (glfw_console->font_textures[i].texture) {
      glDeleteTextures(1, &glfw_console->font_textures[i].texture);
    }
  }
  if (glfw_console->font_textures) {
    free(glfw_console->font_textures);
  }
  if (glfw_console->buffers) {
    for (size_t i = 0; i < glfw_console->buffer_count; i++) {
      if (glfw_console->buffers[i].vertex_data)
        free(glfw_console->buffers[i].vertex_data);
      if (glfw_console->buffers[i].vbo)
        glDeleteBuffers(1, &glfw_console->buffers[i].vbo);
    }
    free(glfw_console->buffers);
  }
  if (glfw_console->vao)
    glDeleteVertexArrays(1, &glfw_console->vao);
  if (glfw_console->shader)
    glDeleteProgram(glfw_console->shader);
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

static void CTUI_glfwKeyCallback(GLFWwindow *window, int key, int scancode,
                                 int action, int mods) {
  CTUI_GlfwGlConsole *glfw_console =
      (CTUI_GlfwGlConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL)
    return;

  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_KEY;
  ev.console = console;
  ev.data.key.key = (CTUI_Key)key;
  ev.data.key.scancode = scancode;
  // NOTE: REPEAT is treated as PRESS for compatibility ncurses
  if (action == GLFW_PRESS || action == GLFW_REPEAT)
    ev.data.key.action = CTUIA_PRESS;
  else
    ev.data.key.action = CTUIA_RELEASE;
  ev.data.key.mods = mods;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwMouseButtonCallback(GLFWwindow *window, int button,
                                         int action, int mods) {
  CTUI_GlfwGlConsole *glfw_console =
      (CTUI_GlfwGlConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL)
    return;
  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_MOUSE_BUTTON;
  ev.console = console;
  ev.data.mouse_button.button = button;
  ev.data.mouse_button.action =
      (action == GLFW_PRESS) ? CTUIA_PRESS : CTUIA_RELEASE;
  ev.data.mouse_button.mods = mods;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwCursorPosCallback(GLFWwindow *window, double xpos,
                                       double ypos) {
  CTUI_GlfwGlConsole *glfw_console =
      (CTUI_GlfwGlConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL)
    return;
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

static void CTUI_glfwScrollCallback(GLFWwindow *window, double xoffset,
                                    double yoffset) {
  CTUI_GlfwGlConsole *glfw_console =
      (CTUI_GlfwGlConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL)
    return;

  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_SCROLL;
  ev.console = console;
  ev.data.scroll.scroll_xy.x = xoffset;
  ev.data.scroll.scroll_xy.y = yoffset;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_glfwFramebufferSizeCallback(GLFWwindow *window, int width,
                                             int height) {
  CTUI_GlfwGlConsole *glfw_console =
      (CTUI_GlfwGlConsole *)glfwGetWindowUserPointer(window);
  if (glfw_console == NULL)
    return;

  glfwMakeContextCurrent(window);
  glViewport(0, 0, width, height);
  CTUI_updateBaseTransform(glfw_console);

  CTUI_Console *console = &glfw_console->base;
  CTUI_Event ev = {0};
  ev.type = CTUI_EVENT_RESIZE;
  ev.console = console;
  ev.data.resize.console_tile_wh = console->_console_tile_wh;
  CTUI_pushEvent(console->_ctx, &ev);
}

static void CTUI_pollEventsGlfwGlConsole(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  if (!glfw_console->window)
    return;
  glfwPollEvents();
  if (glfwWindowShouldClose(glfw_console->window)) { // window close event
    CTUI_Event ev = {0};
    ev.type = CTUI_EVENT_CLOSE;
    ev.console = console;
    CTUI_pushEvent(console->_ctx, &ev);
    glfwSetWindowShouldClose(glfw_console->window, GLFW_FALSE); // so we dont spam events...
  }
}

static void CTUI_transformViewportGlfwGl(CTUI_Console *console, CTUI_FVector2 translation,
                            CTUI_FVector2 scale) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfw_console->viewport_translation = translation;
  glfw_console->viewport_scale = scale;
}

static void CTUI_resetViewportGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfw_console->viewport_translation = (CTUI_FVector2){.x = 0.0f, .y = 0.0f};
  glfw_console->viewport_scale = (CTUI_FVector2){.x = 1.0f, .y = 1.0f};
}

static CTUI_DVector2 CTUI_getCursorViewportPosGlfwGl(CTUI_Console *console) {
  CTUI_DVector2 result = {0, 0};
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwGetCursorPos(glfw_console->window, &result.x, &result.y);
  return result;
}

static CTUI_DVector2 CTUI_getCursorTilePosGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  double px, py;
  glfwGetCursorPos(glfw_console->window, &px, &py);
  
  // Convert to tile coordinates
  double tile_x = px / (double)glfw_console->tile_pixel_wh.x;
  double tile_y = py / (double)glfw_console->tile_pixel_wh.y;
  
  // Apply inverse viewport transform (translation is in normalized coords, scale is direct)
  // The viewport transform in the shader is: pos * scale + translation
  // So inverse is: (pos - translation) / scale
  // But translation is in NDC (-1 to 1), we need to convert
  double console_w = (double)console->_console_tile_wh.x;
  double console_h = (double)console->_console_tile_wh.y;
  
  // Translation in NDC to translation in tiles
  double trans_tiles_x = glfw_console->viewport_translation.x * console_w / 2.0;
  double trans_tiles_y = -glfw_console->viewport_translation.y * console_h / 2.0;
  
  CTUI_DVector2 result;
  result.x = (tile_x - trans_tiles_x) / (double)glfw_console->viewport_scale.x;
  result.y = (tile_y - trans_tiles_y) / (double)glfw_console->viewport_scale.y;
  return result;
}

static int CTUI_getMouseButtonGlfwGl(CTUI_Console *console, int button) {
  if (button < 0 || button > 7) {
    return 0;
  }
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetMouseButton(glfw_console->window, button) == GLFW_PRESS;
}

static int CTUI_getKeyStateGlfwGl(CTUI_Console *console, int key) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetKey(glfw_console->window, key) == GLFW_PRESS;
}

static void CTUI_setWindowPixelWhGlfwGl(CTUI_Console *console, CTUI_IVector2 pixel_wh) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwSetWindowSize(glfw_console->window, pixel_wh.x, pixel_wh.y);
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }
}

static CTUI_IVector2 CTUI_getWindowPixelWhGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  CTUI_IVector2 result;
  glfwGetWindowSize(glfw_console->window, &result.x, &result.y);
  return result;
}

static void CTUI_setViewportTileWhGlfwGl(CTUI_Console *console, CTUI_SVector2 tile_wh) {
  console->_console_tile_wh = tile_wh;
}

static void CTUI_fitWindowPixelWhToViewportTileWhGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
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
  glViewport(0, 0, fb_w, fb_h);
  CTUI_updateBaseTransform(glfw_console);
}

static void CTUI_fitViewportTileWhToWindowPixelWhGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  int win_w, win_h;
  glfwGetWindowSize(glfw_console->window, &win_w, &win_h);
  size_t new_tiles_x = (size_t)((double)win_w / glfw_console->tile_pixel_wh.x);
  size_t new_tiles_y = (size_t)((double)win_h / glfw_console->tile_pixel_wh.y);
  if (new_tiles_x < 1) new_tiles_x = 1;
  if (new_tiles_y < 1) new_tiles_y = 1;
  if (console->_console_tile_wh.x != new_tiles_x || console->_console_tile_wh.y != new_tiles_y) {
    console->_console_tile_wh.x = new_tiles_x;
    console->_console_tile_wh.y = new_tiles_y;
    CTUI_updateBaseTransform(glfw_console);
  }
}

static void CTUI_setWindowResizableGlfwGl(CTUI_Console *console, int resizable) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowResizableGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_RESIZABLE) == GLFW_TRUE;
}

static int CTUI_getIsFullscreenGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfw_console->is_fullscreen;
}

static void CTUI_setWindowDecoratedGlfwGl(CTUI_Console *console, int decorated) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowDecoratedGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_DECORATED) == GLFW_TRUE;
}

static void CTUI_setWindowFloatingGlfwGl(CTUI_Console *console, int floating) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwSetWindowAttrib(glfw_console->window, GLFW_FLOATING, floating ? GLFW_TRUE : GLFW_FALSE);
}

static int CTUI_getWindowFloatingGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_FLOATING) == GLFW_TRUE;
}

static void CTUI_minimizeWindowGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwIconifyWindow(glfw_console->window);
}

static void CTUI_maximizeWindowGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwMaximizeWindow(glfw_console->window);
}

static void CTUI_restoreWindowGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwRestoreWindow(glfw_console->window);
}

static int CTUI_getWindowMinimizedGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_ICONIFIED) == GLFW_TRUE;
}

static int CTUI_getWindowMaximizedGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_MAXIMIZED) == GLFW_TRUE;
}

static void CTUI_focusWindowGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwFocusWindow(glfw_console->window);
}

static int CTUI_getWindowFocusedGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowAttrib(glfw_console->window, GLFW_FOCUSED) == GLFW_TRUE;
}

static void CTUI_requestWindowAttentionGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwRequestWindowAttention(glfw_console->window);
}

static void CTUI_setWindowOpacityGlfwGl(CTUI_Console *console, float opacity) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  glfwSetWindowOpacity(glfw_console->window, opacity);
}

static float CTUI_getWindowOpacityGlfwGl(CTUI_Console *console) {
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  return glfwGetWindowOpacity(glfw_console->window);
}

static CTUI_TuiPlatform CTUI_PLATFORM_GLFW_GL = {
    .is_resizable = 1,
    .destroy = CTUI_destroyGlfwGlConsole,
    .resize = NULL,
    .refresh = CTUI_refreshGlfwGlConsole,
    .pollEvents = CTUI_pollEventsGlfwGlConsole,
    .getCursorViewportPos = CTUI_getCursorViewportPosGlfwGl,
    .getCursorTilePos = CTUI_getCursorTilePosGlfwGl,
    .getMouseButton = CTUI_getMouseButtonGlfwGl,
    .getKeyState = CTUI_getKeyStateGlfwGl,
    .transformViewport = CTUI_transformViewportGlfwGl,
    .resetViewport = CTUI_resetViewportGlfwGl,
    .setWindowPixelWh = CTUI_setWindowPixelWhGlfwGl,
    .getWindowPixelWh = CTUI_getWindowPixelWhGlfwGl,
    .setViewportTileWh = CTUI_setViewportTileWhGlfwGl,
    .fitWindowPixelWhToViewportTileWh = CTUI_fitWindowPixelWhToViewportTileWhGlfwGl,
    .fitViewportTileWhToWindowPixelWh = CTUI_fitViewportTileWhToWindowPixelWhGlfwGl,
    .setWindowResizable = CTUI_setWindowResizableGlfwGl,
    .getWindowResizable = CTUI_getWindowResizableGlfwGl,
    .getIsFullscreen = CTUI_getIsFullscreenGlfwGl,
    .setWindowDecorated = CTUI_setWindowDecoratedGlfwGl,
    .getWindowDecorated = CTUI_getWindowDecoratedGlfwGl,
    .setWindowFloating = CTUI_setWindowFloatingGlfwGl,
    .getWindowFloating = CTUI_getWindowFloatingGlfwGl,
    .minimizeWindow = CTUI_minimizeWindowGlfwGl,
    .maximizeWindow = CTUI_maximizeWindowGlfwGl,
    .restoreWindow = CTUI_restoreWindowGlfwGl,
    .getWindowMinimized = CTUI_getWindowMinimizedGlfwGl,
    .getWindowMaximized = CTUI_getWindowMaximizedGlfwGl,
    .focusWindow = CTUI_focusWindowGlfwGl,
    .getWindowFocused = CTUI_getWindowFocusedGlfwGl,
    .requestWindowAttention = CTUI_requestWindowAttentionGlfwGl,
    .setWindowOpacity = CTUI_setWindowOpacityGlfwGl,
    .getWindowOpacity = CTUI_getWindowOpacityGlfwGl};

CTUI_Console *CTUI_createGlfwOpengl33FakeTerminal(CTUI_Context *ctx,
                                          CTUI_DVector2 tile_pixel_wh,
                                          size_t layer_count,
                                          const CTUI_LayerInfo *layer_infos,
                                          CTUI_ColorMode color_mode,
                                          const char *title) {
  if (glfwInit() == GLFW_FALSE) {
    // TODO
    return NULL;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Start hidden

  // Create a small hidden window initially
  GLFWwindow *window =
      glfwCreateWindow(640, 480, title ? title : "CTUI Console", NULL, NULL);
  if (window == NULL) {
    // TODO
    return NULL;
  }

  CTUI_GlfwGlConsole *glfw_console = calloc(1, sizeof(CTUI_GlfwGlConsole));
  if (glfw_console == NULL) {
    // TODO
    glfwDestroyWindow(window);
    return NULL;
  }

  CTUI_GLFW_CONSOLE_COUNT++;
  glfw_console->window = window;
  glfw_console->tile_pixel_wh = tile_pixel_wh;
  glfw_console->is_fullscreen = 0;
  glfw_console->is_visible = 0;
  glfw_console->viewport_translation = (CTUI_FVector2){.x = 0.0f, .y = 0.0f};
  glfw_console->viewport_scale = (CTUI_FVector2){.x = 1.0f, .y = 1.0f};
  glfw_console->base_transform = CTUI_IDENTITY_MATRIX;

  glfwSetWindowUserPointer(window, glfw_console);
  glfwSetKeyCallback(window, CTUI_glfwKeyCallback);
  glfwSetMouseButtonCallback(window, CTUI_glfwMouseButtonCallback);
  glfwSetCursorPosCallback(window, CTUI_glfwCursorPosCallback);
  glfwSetScrollCallback(window, CTUI_glfwScrollCallback);
  glfwSetFramebufferSizeCallback(window, CTUI_glfwFramebufferSizeCallback);

  glfwMakeContextCurrent(window);
  gladLoadGL(glfwGetProcAddress);

  // Create shader program
  glfw_console->shader = CTUI_createGlProgram(&glfw_console->transform_uniform_loc);

  // Create VAO and buffers
  glGenVertexArrays(1, &glfw_console->vao);
  glBindVertexArray(glfw_console->vao);

  glfw_console->buffer_count = layer_count;
  glfw_console->buffers = calloc(layer_count, sizeof(CTUI_GlfwGlBuffer));
  if (glfw_console->buffers == NULL) {
    // TODO
    CTUI_destroyGlfwGlConsole(&glfw_console->base);
    return NULL;
  }

  for (size_t buffer_i = 0; buffer_i < layer_count; buffer_i++) {
    CTUI_GlfwGlBuffer *buffer = &glfw_console->buffers[buffer_i];
    glGenBuffers(1, &buffer->vbo);
    buffer->vertex_capacity = 0;
    buffer->vertex_count = 0;
    buffer->vertex_data = NULL;
  }

  // Initialize console base
  CTUI_Console *console = &glfw_console->base;
  console->_platform = &CTUI_PLATFORM_GLFW_GL;
  console->_ctx = ctx;
  console->_is_real_terminal = 0;
  console->_effective_color_mode = color_mode;
  console->_console_tile_wh = (CTUI_SVector2){.x = 0, .y = 0};
  console->_layer_count = layer_count;
  console->_layers = calloc(layer_count, sizeof(CTUI_ConsoleLayer));
  if (console->_layers == NULL) {
    // TODO
    CTUI_destroyGlfwGlConsole(console);
    return NULL;
  }

  for (size_t i = 0; i < layer_count; ++i) {
    console->_layers[i]._tile_div_wh = layer_infos[i].tile_div_wh;
    if (console->_layers[i]._tile_div_wh.x == 0) console->_layers[i]._tile_div_wh.x = 1.0;
    if (console->_layers[i]._tile_div_wh.y == 0) console->_layers[i]._tile_div_wh.y = 1.0;
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

void CTUI_hideWindow(CTUI_Console *console) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;
  if (glfw_console->window) {
    glfwHideWindow(glfw_console->window);
    glfw_console->is_visible = 0;
  }
}

void CTUI_setWindowedTileWh(CTUI_Console *console, CTUI_SVector2 console_tile_wh) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;

  // If was fullscreen, exit fullscreen
  if (glfw_console->is_fullscreen) {
    glfwSetWindowMonitor(glfw_console->window, NULL, 100, 100,
                         (int)((double)console_tile_wh.x * glfw_console->tile_pixel_wh.x),
                         (int)((double)console_tile_wh.y * glfw_console->tile_pixel_wh.y), 0);
    glfw_console->is_fullscreen = 0;
  } else {
    int win_w = (int)((double)console_tile_wh.x * glfw_console->tile_pixel_wh.x);
    int win_h = (int)((double)console_tile_wh.y * glfw_console->tile_pixel_wh.y);
    glfwSetWindowSize(glfw_console->window, win_w, win_h);
  }

  console->_console_tile_wh = console_tile_wh;

  // Show window and update transform
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }

  glfwMakeContextCurrent(glfw_console->window);
  int fb_w, fb_h;
  glfwGetFramebufferSize(glfw_console->window, &fb_w, &fb_h);
  glViewport(0, 0, fb_w, fb_h);
  CTUI_updateBaseTransform(glfw_console);

  CTUI_clear(console);
}

void CTUI_setWindowedFullscreen(CTUI_Console *console) {
  if (console->_is_real_terminal) {
    return;
  }
  CTUI_GlfwGlConsole *glfw_console = (CTUI_GlfwGlConsole *)console;

  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = glfwGetVideoMode(monitor);

  // Calculate how many tiles fit in fullscreen
  size_t tiles_x = (size_t)((double)mode->width / glfw_console->tile_pixel_wh.x);
  size_t tiles_y = (size_t)((double)mode->height / glfw_console->tile_pixel_wh.y);

  console->_console_tile_wh.x = tiles_x;
  console->_console_tile_wh.y = tiles_y;

  glfwSetWindowMonitor(glfw_console->window, monitor, 0, 0, mode->width,
                       mode->height, mode->refreshRate);
  glfw_console->is_fullscreen = 1;

  // Show window and update transform
  if (!glfw_console->is_visible) {
    glfwShowWindow(glfw_console->window);
    glfw_console->is_visible = 1;
  }

  glfwMakeContextCurrent(glfw_console->window);
  int fb_w, fb_h;
  glfwGetFramebufferSize(glfw_console->window, &fb_w, &fb_h);
  glViewport(0, 0, fb_w, fb_h);
  CTUI_updateBaseTransform(glfw_console);

  CTUI_clear(console);
}
