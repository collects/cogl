#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int dummy;
} TestState;

static void
paint_legacy (TestState *state)
{
  CoglHandle material = cogl_material_new ();
  CoglColor color;
  GError *error = NULL;
  CoglHandle shader, program;

  cogl_color_init_from_4ub (&color, 0, 0, 0, 255);
  cogl_clear (&color, COGL_BUFFER_BIT_COLOR);

  /* Set the primary vertex color as red */
  cogl_color_set_from_4ub (&color, 0xff, 0x00, 0x00, 0xff);
  cogl_material_set_color (material, &color);

  /* Override the vertex color in the texture environment with a
     constant green color */
  cogl_color_set_from_4ub (&color, 0x00, 0xff, 0x00, 0xff);
  cogl_material_set_layer_combine_constant (material, 0, &color);
  if (!cogl_material_set_layer_combine (material, 0,
                                        "RGBA=REPLACE(CONSTANT)",
                                        &error))
    {
      g_warning ("Error setting blend constant: %s", error->message);
      g_assert_not_reached ();
    }

  /* Set up a dummy vertex shader that does nothing but the usual
     fixed function transform */
  shader = cogl_create_shader (COGL_SHADER_TYPE_VERTEX);
  cogl_shader_source (shader,
                      "void\n"
                      "main ()\n"
                      "{\n"
                      "  cogl_position_out = "
                      "cogl_modelview_projection_matrix * "
                      "cogl_position_in;\n"
                      "  cogl_color_out = cogl_color_in;\n"
                      "}\n");
  cogl_shader_compile (shader);
  if (!cogl_shader_is_compiled (shader))
    {
      char *log = cogl_shader_get_info_log (shader);
      g_warning ("Shader compilation failed:\n%s", log);
      g_free (log);
      g_assert_not_reached ();
    }

  program = cogl_create_program ();
  cogl_program_attach_shader (program, shader);
  cogl_program_link (program);

  cogl_handle_unref (shader);

  /* Draw something using the material */
  cogl_set_source (material);
  cogl_rectangle (0, 0, 50, 50);

  /* Draw it again using the program. It should look exactly the same */
  cogl_program_use (program);
  cogl_rectangle (50, 0, 100, 50);
  cogl_program_use (COGL_INVALID_HANDLE);

  cogl_handle_unref (material);
  cogl_handle_unref (program);
}

static void
paint (TestState *state)
{
  CoglPipeline *pipeline = cogl_pipeline_new ();
  CoglColor color;
  GError *error = NULL;
  CoglHandle shader, program;

  cogl_color_init_from_4ub (&color, 0, 0, 0, 255);
  cogl_clear (&color, COGL_BUFFER_BIT_COLOR);

  /* Set the primary vertex color as red */
  cogl_color_set_from_4ub (&color, 0xff, 0x00, 0x00, 0xff);
  cogl_pipeline_set_color (pipeline, &color);

  /* Override the vertex color in the texture environment with a
     constant green color */
  cogl_color_set_from_4ub (&color, 0x00, 0xff, 0x00, 0xff);
  cogl_pipeline_set_layer_combine_constant (pipeline, 0, &color);
  if (!cogl_pipeline_set_layer_combine (pipeline, 0,
                                        "RGBA=REPLACE(CONSTANT)",
                                        &error))
    {
      g_warning ("Error setting blend constant: %s", error->message);
      g_assert_not_reached ();
    }

  /* Set up a dummy vertex shader that does nothing but the usual
     fixed function transform */
  shader = cogl_create_shader (COGL_SHADER_TYPE_VERTEX);
  cogl_shader_source (shader,
                      "void\n"
                      "main ()\n"
                      "{\n"
                      "  cogl_position_out = "
                      "cogl_modelview_projection_matrix * "
                      "cogl_position_in;\n"
                      "  cogl_color_out = cogl_color_in;\n"
                      "}\n");
  cogl_shader_compile (shader);
  if (!cogl_shader_is_compiled (shader))
    {
      char *log = cogl_shader_get_info_log (shader);
      g_warning ("Shader compilation failed:\n%s", log);
      g_free (log);
      g_assert_not_reached ();
    }

  program = cogl_create_program ();
  cogl_program_attach_shader (program, shader);
  cogl_program_link (program);

  cogl_handle_unref (shader);

  /* Draw something without the program */
  cogl_set_source (pipeline);
  cogl_rectangle (0, 0, 50, 50);

  /* Draw it again using the program. It should look exactly the same */
  cogl_pipeline_set_user_program (pipeline, program);
  cogl_handle_unref (program);

  cogl_rectangle (50, 0, 100, 50);
  cogl_pipeline_set_user_program (pipeline, COGL_INVALID_HANDLE);

  cogl_object_unref (pipeline);
}

static void
check_pixel (int x, int y, guint8 r, guint8 g, guint8 b)
{
  guint32 pixel;
  char *screen_pixel;
  char *intended_pixel;

  cogl_read_pixels (x, y, 1, 1, COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    (guint8 *) &pixel);

  screen_pixel = g_strdup_printf ("#%06x", GUINT32_FROM_BE (pixel) >> 8);
  intended_pixel = g_strdup_printf ("#%02x%02x%02x", r, g, b);

  g_assert_cmpstr (screen_pixel, ==, intended_pixel);

  g_free (screen_pixel);
  g_free (intended_pixel);
}

static void
validate_result (void)
{
  /* Non-shader version */
  check_pixel (25, 25, 0, 0xff, 0);
  /* Shader version */
  check_pixel (75, 25, 0, 0xff, 0);
}

void
test_cogl_just_vertex_shader (TestUtilsGTestFixture *fixture,
                              void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;

  cogl_ortho (0, cogl_framebuffer_get_width (shared_state->fb), /* left, right */
              cogl_framebuffer_get_height (shared_state->fb), 0, /* bottom, top */
              -1, 100 /* z near, far */);

  /* If shaders aren't supported then we can't run the test */
  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    {
      paint_legacy (&state);
      validate_result ();

      paint (&state);
      validate_result ();

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}

