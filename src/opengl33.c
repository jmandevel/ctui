#include <ctui/ctui.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct CTUI_GL33Vertex {
  float x, y;
  float u, v, page;
  float fg[4];
  float bg[4];
} CTUI_GL33Vertex;

typedef struct CTUI_GL33Buffer {
  GLuint vbo;
  size_t vertex_count;
  size_t vertex_capacity;
  CTUI_GL33Vertex *vertex_data;
} CTUI_GL33Buffer;

typedef struct CTUI_GL33FontTexture {
  CTUI_Font *font;
  GLuint texture;
} CTUI_GL33FontTexture;

typedef struct CTUI_OpenGL33Renderer {
  CTUI_Renderer base;
  GLuint shader;
  GLint transform_uniform_loc;
  GLuint vao;
  size_t buffer_count;
  CTUI_GL33Buffer *buffers;
  size_t font_texture_count;
  CTUI_GL33FontTexture *font_textures;
  float transform[16];
  int is_gl_loaded;
} CTUI_OpenGL33Renderer;

static const char *GL33_VERTEX_SHADER_SRC =
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

static const char *GL33_FRAGMENT_SHADER_SRC =
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

static GLuint CTUI_gl33CompileShader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  return shader;
}

static GLuint CTUI_gl33CreateProgram(GLint *out_transform_loc) {
  GLuint vs = CTUI_gl33CompileShader(GL_VERTEX_SHADER, GL33_VERTEX_SHADER_SRC);
  GLuint fs =
      CTUI_gl33CompileShader(GL_FRAGMENT_SHADER, GL33_FRAGMENT_SHADER_SRC);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  *out_transform_loc = glGetUniformLocation(prog, "u_transform");
  return prog;
}

static GLuint CTUI_gl33CreateFontTexture(CTUI_Font *font) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
  size_t width = CTUI_getFontImageWidth(font);
  size_t height = CTUI_getFontImageHeight(font);
  size_t pages = CTUI_getFontImagePages(font);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, pages, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, font->_image._pixels);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

static int CTUI_gl33Init(CTUI_Renderer *renderer) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  if (!gl->is_gl_loaded) {
    return -1;
  }
  gl->shader = CTUI_gl33CreateProgram(&gl->transform_uniform_loc);
  glGenVertexArrays(1, &gl->vao);
  glBindVertexArray(gl->vao);
  memset(gl->transform, 0, sizeof(gl->transform));
  gl->transform[0] = 1.0f;
  gl->transform[5] = 1.0f;
  gl->transform[10] = 1.0f;
  gl->transform[15] = 1.0f;
  return 0;
}

static void CTUI_gl33Resize(CTUI_Renderer *renderer, int width, int height) {
  (void)renderer;
  glViewport(0, 0, width, height);
}

static void *CTUI_gl33GetOrCreateFontTexture(CTUI_Renderer *renderer,
                                             CTUI_Font *font) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  for (size_t i = 0; i < gl->font_texture_count; i++) {
    if (gl->font_textures[i].font == font) {
      return (void *)(uintptr_t)gl->font_textures[i].texture;
    }
  }
  GLuint texture = CTUI_gl33CreateFontTexture(font);
  size_t new_count = gl->font_texture_count + 1;
  CTUI_GL33FontTexture *new_textures =
      realloc(gl->font_textures, sizeof(CTUI_GL33FontTexture) * new_count);
  if (new_textures == NULL) {
    glDeleteTextures(1, &texture);
    return NULL;
  }
  gl->font_textures = new_textures;
  gl->font_textures[gl->font_texture_count].font = font;
  gl->font_textures[gl->font_texture_count].texture = texture;
  gl->font_texture_count = new_count;
  return (void *)(uintptr_t)texture;
}

static void CTUI_gl33FreeFontTexture(CTUI_Renderer *renderer,
                                     void *texture_handle) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  GLuint texture = (GLuint)(uintptr_t)texture_handle;
  for (size_t i = 0; i < gl->font_texture_count; i++) {
    if (gl->font_textures[i].texture == texture) {
      glDeleteTextures(1, &texture);
      for (size_t j = i; j < gl->font_texture_count - 1; j++) {
        gl->font_textures[j] = gl->font_textures[j + 1];
      }
      gl->font_texture_count--;
      return;
    }
  }
}

static void CTUI_gl33SetTransform(CTUI_Renderer *renderer,
                                  const float *matrix4x4) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  memcpy(gl->transform, matrix4x4, sizeof(gl->transform));
}

static void CTUI_gl33MakeCurrent(CTUI_Renderer *renderer) { (void)renderer; }

static void CTUI_gl33EnsureBuffers(CTUI_OpenGL33Renderer *gl,
                                   size_t layer_count) {
  if (gl->buffer_count >= layer_count) {
    return;
  }
  CTUI_GL33Buffer *new_buffers =
      realloc(gl->buffers, sizeof(CTUI_GL33Buffer) * layer_count);
  if (new_buffers == NULL) {
    return;
  }
  gl->buffers = new_buffers;
  for (size_t i = gl->buffer_count; i < layer_count; i++) {
    gl->buffers[i].vbo = 0;
    gl->buffers[i].vertex_count = 0;
    gl->buffers[i].vertex_capacity = 0;
    gl->buffers[i].vertex_data = NULL;
    glGenBuffers(1, &gl->buffers[i].vbo);
  }
  gl->buffer_count = layer_count;
}

static void CTUI_gl33Render(CTUI_Renderer *renderer, CTUI_Console *console) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  CTUI_SVector2 console_tile_wh = CTUI_getConsoleTileWh(console);
  if (console_tile_wh.x == 0 || console_tile_wh.y == 0) {
    return;
  }
  size_t layer_count = CTUI_getConsoleLayerCount(console);
  CTUI_gl33EnsureBuffers(gl, layer_count);
  for (size_t buffer_i = 0; buffer_i < layer_count; buffer_i++) {
    CTUI_GL33Buffer *buffer = &gl->buffers[buffer_i];
    buffer->vertex_count = 0;
    CTUI_ConsoleLayer *layer = CTUI_getConsoleLayer(console, buffer_i);
    if (layer == NULL)
      continue;
    const CTUI_Font *font = CTUI_getLayerFont(layer);
    if (font == NULL)
      continue;
    CTUI_DVector2 tile_div_wh = CTUI_getLayerTileDivWh(layer);
    if (tile_div_wh.x == 0 || tile_div_wh.y == 0)
      continue;
    float tile_screen_w =
        2.0f / (float)((double)console_tile_wh.x * tile_div_wh.x);
    float tile_screen_h =
        2.0f / (float)((double)console_tile_wh.y * tile_div_wh.y);
    size_t tiles_count = CTUI_getLayerTilesCount(layer);
    if (buffer->vertex_capacity < tiles_count * 6) {
      CTUI_GL33Vertex *new_data = realloc(
          buffer->vertex_data, tiles_count * 6 * sizeof(CTUI_GL33Vertex));
      if (new_data == NULL)
        continue;
      buffer->vertex_data = new_data;
      buffer->vertex_capacity = tiles_count * 6;
    }
    for (size_t tile_i = 0; tile_i < tiles_count; tile_i++) {
      CTUI_ConsoleTile *tile = &layer->_tiles[tile_i];

      float left_x = ((float)tile->_pos_xy.x * tile_screen_w) - 1.0f;
      float right_x = left_x + tile_screen_w;
      float top_y = 1.0f - ((float)tile->_pos_xy.y * tile_screen_h);
      float bottom_y = top_y - tile_screen_h;
      CTUI_Glyph *glyph = CTUI_tryGetGlyph((CTUI_Font *)font, tile->_codepoint);
      if (glyph == NULL) {
        // TODO error glyph
        continue;
      }
      CTUI_Stpqp tex_coords = CTUI_getGlyphTexCoords(glyph);
      CTUI_ColorRgba32 fg_rgba = CTUI_convertToRgba32(tile->_fg);
      CTUI_ColorRgba32 bg_rgba = CTUI_convertToRgba32(tile->_bg);
      float fg_r = (float)fg_rgba.r / 255.0f;
      float fg_g = (float)fg_rgba.g / 255.0f;
      float fg_b = (float)fg_rgba.b / 255.0f;
      float fg_a = (float)fg_rgba.a / 255.0f;
      float bg_r = (float)bg_rgba.r / 255.0f;
      float bg_g = (float)bg_rgba.g / 255.0f;
      float bg_b = (float)bg_rgba.b / 255.0f;
      float bg_a = (float)bg_rgba.a / 255.0f;
      CTUI_GL33Vertex *v0 = &buffer->vertex_data[buffer->vertex_count++];
      v0->x = left_x;
      v0->y = top_y;
      v0->u = tex_coords.s;
      v0->v = tex_coords.p;
      v0->page = tex_coords.page;
      v0->fg[0] = fg_r;
      v0->fg[1] = fg_g;
      v0->fg[2] = fg_b;
      v0->fg[3] = fg_a;
      v0->bg[0] = bg_r;
      v0->bg[1] = bg_g;
      v0->bg[2] = bg_b;
      v0->bg[3] = bg_a;
      CTUI_GL33Vertex *v1 = &buffer->vertex_data[buffer->vertex_count++];
      v1->x = right_x;
      v1->y = top_y;
      v1->u = tex_coords.t;
      v1->v = tex_coords.p;
      v1->page = tex_coords.page;
      v1->fg[0] = fg_r;
      v1->fg[1] = fg_g;
      v1->fg[2] = fg_b;
      v1->fg[3] = fg_a;
      v1->bg[0] = bg_r;
      v1->bg[1] = bg_g;
      v1->bg[2] = bg_b;
      v1->bg[3] = bg_a;
      CTUI_GL33Vertex *v2 = &buffer->vertex_data[buffer->vertex_count++];
      v2->x = left_x;
      v2->y = bottom_y;
      v2->u = tex_coords.s;
      v2->v = tex_coords.q;
      v2->page = tex_coords.page;
      v2->fg[0] = fg_r;
      v2->fg[1] = fg_g;
      v2->fg[2] = fg_b;
      v2->fg[3] = fg_a;
      v2->bg[0] = bg_r;
      v2->bg[1] = bg_g;
      v2->bg[2] = bg_b;
      v2->bg[3] = bg_a;
      CTUI_GL33Vertex *v3 = &buffer->vertex_data[buffer->vertex_count++];
      v3->x = right_x;
      v3->y = top_y;
      v3->u = tex_coords.t;
      v3->v = tex_coords.p;
      v3->page = tex_coords.page;
      v3->fg[0] = fg_r;
      v3->fg[1] = fg_g;
      v3->fg[2] = fg_b;
      v3->fg[3] = fg_a;
      v3->bg[0] = bg_r;
      v3->bg[1] = bg_g;
      v3->bg[2] = bg_b;
      v3->bg[3] = bg_a;
      CTUI_GL33Vertex *v4 = &buffer->vertex_data[buffer->vertex_count++];
      v4->x = right_x;
      v4->y = bottom_y;
      v4->u = tex_coords.t;
      v4->v = tex_coords.q;
      v4->page = tex_coords.page;
      v4->fg[0] = fg_r;
      v4->fg[1] = fg_g;
      v4->fg[2] = fg_b;
      v4->fg[3] = fg_a;
      v4->bg[0] = bg_r;
      v4->bg[1] = bg_g;
      v4->bg[2] = bg_b;
      v4->bg[3] = bg_a;
      CTUI_GL33Vertex *v5 = &buffer->vertex_data[buffer->vertex_count++];
      v5->x = left_x;
      v5->y = bottom_y;
      v5->u = tex_coords.s;
      v5->v = tex_coords.q;
      v5->page = tex_coords.page;
      v5->fg[0] = fg_r;
      v5->fg[1] = fg_g;
      v5->fg[2] = fg_b;
      v5->fg[3] = fg_a;
      v5->bg[0] = bg_r;
      v5->bg[1] = bg_g;
      v5->bg[2] = bg_b;
      v5->bg[3] = bg_a;
    }
  }
  if (console->_fill_bg_set) {
    CTUI_ColorRgba32 fill_rgba = CTUI_convertToRgba32(console->_fill_bg_color);
    float r = (float)fill_rgba.r / 255.0f;
    float g = (float)fill_rgba.g / 255.0f;
    float b = (float)fill_rgba.b / 255.0f;
    float a = (float)fill_rgba.a / 255.0f;
    glClearColor(r, g, b, a);
  } else {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  }
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(gl->shader);
  glUniformMatrix4fv(gl->transform_uniform_loc, 1, GL_FALSE, gl->transform);
  glBindVertexArray(gl->vao);
  for (size_t buffer_i = 0; buffer_i < layer_count; buffer_i++) {
    CTUI_GL33Buffer *buffer = &gl->buffers[buffer_i];
    if (buffer->vertex_count == 0)
      continue;
    CTUI_ConsoleLayer *layer = CTUI_getConsoleLayer(console, buffer_i);
    if (layer == NULL)
      continue;
    const CTUI_Font *font = CTUI_getLayerFont(layer);
    if (font == NULL)
      continue;
    GLuint texture = (GLuint)(uintptr_t)CTUI_gl33GetOrCreateFontTexture(
        renderer, (CTUI_Font *)font);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(CTUI_GL33Vertex) * buffer->vertex_count,
                 buffer->vertex_data, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CTUI_GL33Vertex),
                          (void *)offsetof(CTUI_GL33Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CTUI_GL33Vertex),
                          (void *)offsetof(CTUI_GL33Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(CTUI_GL33Vertex),
                          (void *)offsetof(CTUI_GL33Vertex, fg));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(CTUI_GL33Vertex),
                          (void *)offsetof(CTUI_GL33Vertex, bg));
    glDrawArrays(GL_TRIANGLES, 0, buffer->vertex_count);
  }
}

static const CTUI_RendererVtable CTUI_GL33_VTABLE = {
    .init = CTUI_gl33Init,
    .resize = CTUI_gl33Resize,
    .render = CTUI_gl33Render,
    .getOrCreateFontTexture = CTUI_gl33GetOrCreateFontTexture,
    .freeFontTexture = CTUI_gl33FreeFontTexture,
    .setTransform = CTUI_gl33SetTransform,
    .makeCurrent = CTUI_gl33MakeCurrent,
};

CTUI_Renderer *
CTUI_createOpenGL33Renderer(CTUI_GLGetProcAddress getProcAddress) {
  CTUI_OpenGL33Renderer *gl = calloc(1, sizeof(CTUI_OpenGL33Renderer));
  if (gl == NULL) {
    return NULL;
  }
  gl->base.vtable = &CTUI_GL33_VTABLE;
  if (gladLoadGL((GLADloadfunc)getProcAddress) == 0) {
    free(gl);
    return NULL;
  }
  gl->is_gl_loaded = 1;
  return &gl->base;
}

void CTUI_destroyOpenGL33Renderer(CTUI_Renderer *renderer) {
  CTUI_OpenGL33Renderer *gl = (CTUI_OpenGL33Renderer *)renderer;
  for (size_t i = 0; i < gl->font_texture_count; i++) {
    if (gl->font_textures[i].texture) {
      glDeleteTextures(1, &gl->font_textures[i].texture);
    }
  }
  if (gl->font_textures) {
    free(gl->font_textures);
  }
  if (gl->buffers) {
    for (size_t i = 0; i < gl->buffer_count; i++) {
      if (gl->buffers[i].vertex_data) {
        free(gl->buffers[i].vertex_data);
      }
      if (gl->buffers[i].vbo) {
        glDeleteBuffers(1, &gl->buffers[i].vbo);
      }
    }
    free(gl->buffers);
  }
  if (gl->vao) {
    glDeleteVertexArrays(1, &gl->vao);
  }
  if (gl->shader) {
    glDeleteProgram(gl->shader);
  }
  free(renderer);
}
