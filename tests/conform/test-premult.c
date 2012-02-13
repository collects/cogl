
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

typedef struct _TestState
{
  ClutterGeometry stage_geom;
  CoglHandle passthrough_material;
} TestState;

static guchar *
gen_tex_data (guint32 color)
{
  guchar *tex_data, *p;
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);

  tex_data = g_malloc (QUAD_WIDTH * QUAD_WIDTH * 4);

  for (p = tex_data + QUAD_WIDTH * QUAD_WIDTH * 4; p > tex_data;)
    {
      *(--p) = a;
      *(--p) = b;
      *(--p) = g;
      *(--p) = r;
    }

  return tex_data;
}

static CoglHandle
make_texture (guint32 color,
              CoglPixelFormat src_format,
              CoglPixelFormat internal_format)
{
  CoglHandle tex;
  guchar *tex_data = gen_tex_data (color);

  tex = cogl_texture_new_from_data (QUAD_WIDTH,
                                    QUAD_WIDTH,
                                    COGL_TEXTURE_NONE,
                                    src_format,
                                    internal_format,
                                    QUAD_WIDTH * 4,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

static void
check_texture (TestState *state,
               int x,
               int y,
               CoglHandle tex,
               guint32 expected_result)
{
  guchar        pixel[4];
  int           y_off;
  int           x_off;

  cogl_material_set_layer (state->passthrough_material, 0, tex);

  cogl_set_source (state->passthrough_material);
  cogl_rectangle (x * QUAD_WIDTH,
                  y * QUAD_WIDTH,
                  x * QUAD_WIDTH + QUAD_WIDTH,
                  y * QUAD_WIDTH + QUAD_WIDTH);

  test_utils_check_pixel (x_off, y_off, expected_result);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglHandle tex;
  guchar *tex_data;

  /* If the user explicitly specifies an unmultiplied internal format then
   * Cogl shouldn't automatically premultiply the given texture data... */
  if (g_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  check_texture (state, 0, 0, /* position */
                 tex,
                 0xff00ff80); /* expected */

  /* If the user explicitly requests a premultiplied internal format and
   * gives unmultiplied src data then Cogl should always premultiply that
   * for us */
  if (g_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  check_texture (state, 1, 0, /* position */
                 tex,
                 0x80008080); /* expected */

  /* If the user gives COGL_PIXEL_FORMAT_ANY for the internal format then
   * by default Cogl should premultiply the given texture data...
   * (In the future there will be additional Cogl API to control this
   *  behaviour) */
  if (g_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = ANY)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_ANY); /* internal format */
  check_texture (state, 2, 0, /* position */
                 tex,
                 0x80008080); /* expected */

  /* If the user requests a premultiplied internal texture format and supplies
   * premultiplied source data, Cogl should never modify that source data...
   */
  if (g_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  check_texture (state, 3, 0, /* position */
                 tex,
                 0x80008080); /* expected */

  /* If the user requests an unmultiplied internal texture format, but
   * supplies premultiplied source data, then Cogl should always
   * un-premultiply the source data... */
  if (g_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, internal = RGBA_8888)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  check_texture (state, 4, 0, /* position */
                 tex,
                 0xff00ff80); /* expected */

  /* If the user allows any internal texture format and provides premultipled
   * source data then by default Cogl shouldn't modify the source data...
   * (In the future there will be additional Cogl API to control this
   *  behaviour) */
  if (g_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, internal = ANY)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_ANY); /* internal format */
  check_texture (state, 5, 0, /* position */
                 tex,
                 0x80008080); /* expected */

  /*
   * Test cogl_texture_set_region() ....
   */

  if (g_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  if (g_test_verbose ())
    g_print ("set_region (0xff00ff80, RGBA_8888)\n");
  tex_data = gen_tex_data (0xff00ff80);
  cogl_texture_set_region (tex,
                           0, 0, /* src x, y */
                           0, 0, /* dst x, y */
                           QUAD_WIDTH, QUAD_WIDTH, /* dst width, height */
                           QUAD_WIDTH, QUAD_WIDTH, /* src width, height */
                           COGL_PIXEL_FORMAT_RGBA_8888,
                           0, /* auto compute row stride */
                           tex_data);
  check_texture (state, 6, 0, /* position */
                 tex,
                 0xff00ff80); /* expected */

  /* Updating a texture region for an unmultiplied texture using premultiplied
   * region data should result in Cogl unmultiplying the given region data...
   */
  if (g_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  if (g_test_verbose ())
    g_print ("set_region (0x80008080, RGBA_8888_PRE)\n");
  tex_data = gen_tex_data (0x80008080);
  cogl_texture_set_region (tex,
                           0, 0, /* src x, y */
                           0, 0, /* dst x, y */
                           QUAD_WIDTH, QUAD_WIDTH, /* dst width, height */
                           QUAD_WIDTH, QUAD_WIDTH, /* src width, height */
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                           0, /* auto compute row stride */
                           tex_data);
  check_texture (state, 7, 0, /* position */
                 tex,
                 0xff00ff80); /* expected */


  if (g_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  if (g_test_verbose ())
    g_print ("set_region (0x80008080, RGBA_8888_PRE)\n");
  tex_data = gen_tex_data (0x80008080);
  cogl_texture_set_region (tex,
                           0, 0, /* src x, y */
                           0, 0, /* dst x, y */
                           QUAD_WIDTH, QUAD_WIDTH, /* dst width, height */
                           QUAD_WIDTH, QUAD_WIDTH, /* src width, height */
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                           0, /* auto compute row stride */
                           tex_data);
  check_texture (state, 8, 0, /* position */
                 tex,
                 0x80008080); /* expected */


  /* Updating a texture region for a premultiplied texture using unmultiplied
   * region data should result in Cogl premultiplying the given region data...
   */
  if (g_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  if (g_test_verbose ())
    g_print ("set_region (0xff00ff80, RGBA_8888)\n");
  tex_data = gen_tex_data (0xff00ff80);
  cogl_texture_set_region (tex,
                           0, 0, /* src x, y */
                           0, 0, /* dst x, y */
                           QUAD_WIDTH, QUAD_WIDTH, /* dst width, height */
                           QUAD_WIDTH, QUAD_WIDTH, /* src width, height */
                           COGL_PIXEL_FORMAT_RGBA_8888,
                           0, /* auto compute row stride */
                           tex_data);
  check_texture (state, 9, 0, /* position */
                 tex,
                 0x80008080); /* expected */

  /* Comment this out if you want visual feedback for what this test paints */
  clutter_main_quit ();
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_premult (TestUtilsGTestFixture *fixture,
                   void *data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  unsigned int idle_source;

  state.passthrough_material = cogl_material_new ();
  cogl_material_set_blend (state.passthrough_material,
                           "RGBA = ADD (SRC_COLOR, 0)", NULL);
  cogl_material_set_layer_combine (state.passthrough_material, 0,
                                   "RGBA = REPLACE (TEXTURE)", NULL);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing incase someone comments out the
   * clutter_main_quit and wants visual feedback for the test since we
   * wont be doing anything else that will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

