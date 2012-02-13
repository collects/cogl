#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int dummy;
} TestState;

static CoglHandle
create_dummy_texture (void)
{
  /* Create a dummy 1x1 green texture to replace the color from the
     vertex shader */
  static const guint8 data[4] = { 0x00, 0xff, 0x00, 0xff };

  return cogl_texture_new_from_data (1, 1, /* size */
                                     COGL_TEXTURE_NONE,
                                     COGL_PIXEL_FORMAT_RGB_888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     4, /* rowstride */
                                     data);
}

static void
paint_legacy (TestState *state)
{
  CoglHandle material = cogl_material_new ();
  CoglHandle tex;
  CoglColor color;
  GError *error = NULL;
  CoglHandle shader, program;

  cogl_color_init_from_4ub (&color, 0, 0, 0, 255);
  cogl_clear (&color, COGL_BUFFER_BIT_COLOR);

  /* Set the primary vertex color as red */
  cogl_color_set_from_4ub (&color, 0xff, 0x00, 0x00, 0xff);
  cogl_material_set_color (material, &color);

  /* Override the vertex color in the texture environment with a
     constant green color provided by a texture */
  tex = create_dummy_texture ();
  cogl_material_set_layer (material, 0, tex);
  cogl_handle_unref (tex);
  if (!cogl_material_set_layer_combine (material, 0,
                                        "RGBA=REPLACE(TEXTURE)",
                                        &error))
    {
      g_warning ("Error setting layer combine: %s", error->message);
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
                      "  cogl_tex_coord_out[0] = cogl_tex_coord_in;\n"
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
  CoglHandle tex;
  CoglColor color;
  GError *error = NULL;
  CoglHandle shader, program;

  cogl_color_init_from_4ub (&color, 0, 0, 0, 255);
  cogl_clear (&color, COGL_BUFFER_BIT_COLOR);

  /* Set the primary vertex color as red */
  cogl_color_set_from_4ub (&color, 0xff, 0x00, 0x00, 0xff);
  cogl_pipeline_set_color (pipeline, &color);

  /* Override the vertex color in the texture environment with a
     constant green color provided by a texture */
  tex = create_dummy_texture ();
  cogl_pipeline_set_layer_texture (pipeline, 0, tex);
  cogl_handle_unref (tex);
  if (!cogl_pipeline_set_layer_combine (pipeline, 0,
                                        "RGBA=REPLACE(TEXTURE)",
                                        &error))
    {
      g_warning ("Error setting layer combine: %s", error->message);
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
                      "  cogl_tex_coord_out[0] = cogl_tex_coord_in;\n"
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
validate_result (void)
{
  /* Non-shader version */
  test_utils_check_pixel (25, 25, 0x00ff0000);
  /* Shader version */
  test_utils_check_pixel (75, 25, 0x00ff0000);
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

