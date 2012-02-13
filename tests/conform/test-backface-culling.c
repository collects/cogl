#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

/* Size the texture so that it is just off a power of two to encourage
   it so use software tiling when NPOTs aren't available */
#define TEXTURE_SIZE        257

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET          2

/* Size to actually render the texture at */
#define TEXTURE_RENDER_SIZE 8

typedef struct _TestState
{
  CoglFramebuffer *fb;
  CoglHandle texture;
  CoglFramebuffer *offscreen;
  CoglHandle offscreen_tex;
  int width, height;
} TestState;

static void
validate_part (int xnum, int ynum, gboolean shown)
{
  test_utils_check_region (xnum * TEXTURE_RENDER_SIZE + TEST_INSET,
                           ynum * TEXTURE_RENDER_SIZE + TEST_INSET,
                           TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                           TEXTURE_RENDER_SIZE - TEST_INSET * 2,
                           shown ? 0xff0000ff : 0x000000ff);
}

/* We draw everything 16 times. The draw number is used as a bitmask
   to test all of the combinations of enabling legacy state, both
   winding orders and all four culling modes */

#define USE_LEGACY_STATE(draw_num) (((draw_num) & 0x01) >> 0)
#define FRONT_WINDING(draw_num)    (((draw_num) & 0x02) >> 1)
#define CULL_FACE_MODE(draw_num)   (((draw_num) & 0x0c) >> 2)

static void
paint_test_backface_culling (TestState *state)
{
  int draw_num;
  CoglPipeline *base_pipeline = cogl_pipeline_new ();
  CoglColor clear_color;

  cogl_ortho (0, state->width, /* left, right */
              state->height, 0, /* bottom, top */
              -1, 100 /* z near, far */);

  cogl_color_init_from_4ub (&clear_color, 0x00, 0x00, 0x00, 0xff);
  cogl_clear (&clear_color, COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_STENCIL);

  cogl_pipeline_set_layer_texture (base_pipeline, 0, state->texture);

  cogl_pipeline_set_layer_filters (base_pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  /* Render the scene sixteen times to test all of the combinations of
     cull face mode, legacy state and winding orders */
  for (draw_num = 0; draw_num < 16; draw_num++)
    {
      float x1 = 0, x2, y1 = 0, y2 = (float)(TEXTURE_RENDER_SIZE);
      CoglTextureVertex verts[4];
      CoglPipeline *pipeline;

      cogl_push_matrix ();
      cogl_translate (0, TEXTURE_RENDER_SIZE * draw_num, 0);

      pipeline = cogl_pipeline_copy (base_pipeline);

      cogl_set_backface_culling_enabled (USE_LEGACY_STATE (draw_num));
      cogl_pipeline_set_front_face_winding (pipeline, FRONT_WINDING (draw_num));
      cogl_pipeline_set_cull_face_mode (pipeline, CULL_FACE_MODE (draw_num));

      cogl_push_source (pipeline);

      memset (verts, 0, sizeof (verts));

      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture */
      cogl_rectangle (x1, y1, x2, y2);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a front-facing texture with flipped texcoords */
      cogl_rectangle_with_texture_coords (x1, y1, x2, y2,
                                          1.0, 0.0, 0.0, 1.0);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a back-facing texture */
      cogl_rectangle (x2, y1, x1, y2);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* If the texture is sliced then cogl_polygon doesn't work so
         we'll just use a solid color instead */
      if (cogl_texture_is_sliced (state->texture))
        cogl_set_source_color4ub (255, 0, 0, 255);

      /* Draw a front-facing polygon */
      verts[0].x = x1;    verts[0].y = y2;
      verts[1].x = x2;    verts[1].y = y2;
      verts[2].x = x2;    verts[2].y = y1;
      verts[3].x = x1;    verts[3].y = y1;
      verts[0].tx = 0;    verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;    verts[3].ty = 1.0;
      cogl_polygon (verts, 4, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      /* Draw a back-facing polygon */
      verts[0].x = x1;    verts[0].y = y1;
      verts[1].x = x2;    verts[1].y = y1;
      verts[2].x = x2;    verts[2].y = y2;
      verts[3].x = x1;    verts[3].y = y2;
      verts[0].tx = 0;    verts[0].ty = 0;
      verts[1].tx = 1.0;  verts[1].ty = 0;
      verts[2].tx = 1.0;  verts[2].ty = 1.0;
      verts[3].tx = 0;    verts[3].ty = 1.0;
      cogl_polygon (verts, 4, FALSE);

      x1 = x2;
      x2 = x1 + (float)(TEXTURE_RENDER_SIZE);

      cogl_pop_matrix ();

      cogl_pop_source ();
      cogl_object_unref (pipeline);
    }

  cogl_object_unref (base_pipeline);
}

static void
validate_result (int y_offset)
{
  int draw_num;

  for (draw_num = 0; draw_num < 16; draw_num++)
    {
      gboolean cull_front, cull_back;
      CoglPipelineCullFaceMode cull_mode;

      if (USE_LEGACY_STATE (draw_num))
        cull_mode = COGL_PIPELINE_CULL_FACE_MODE_BACK;
      else
        cull_mode = CULL_FACE_MODE (draw_num);

      switch (cull_mode)
        {
        case COGL_PIPELINE_CULL_FACE_MODE_NONE:
          cull_front = FALSE;
          cull_back = FALSE;
          break;

        case COGL_PIPELINE_CULL_FACE_MODE_FRONT:
          cull_front = TRUE;
          cull_back = FALSE;
          break;

        case COGL_PIPELINE_CULL_FACE_MODE_BACK:
          cull_front = FALSE;
          cull_back = TRUE;
          break;

        case COGL_PIPELINE_CULL_FACE_MODE_BOTH:
          cull_front = TRUE;
          cull_back = TRUE;
          break;
        }

      if (FRONT_WINDING (draw_num) == COGL_WINDING_CLOCKWISE)
        {
          gboolean tmp = cull_front;
          cull_front = cull_back;
          cull_back = tmp;
        }

      /* Front-facing texture */
      validate_part (0, y_offset + draw_num, !cull_front);
      /* Front-facing texture with flipped tex coords */
      validate_part (1, y_offset + draw_num, !cull_front);
      /* Back-facing texture */
      validate_part (2, y_offset + draw_num, !cull_back);
      /* Front-facing texture polygon */
      validate_part (3, y_offset + draw_num, !cull_front);
      /* Back-facing texture polygon */
      validate_part (4, y_offset + draw_num, !cull_back);
    }
}

static void
paint (TestState *state)
{
  float stage_viewport[4];
  CoglMatrix stage_projection;
  CoglMatrix stage_modelview;

  paint_test_backface_culling (state);

  /*
   * Now repeat the test but rendered to an offscreen
   * framebuffer. Note that by default the conformance tests are
   * always run to an offscreen buffer but we might as well have this
   * check anyway in case it is being run with COGL_TEST_ONSCREEN=1
   */

  cogl_get_viewport (stage_viewport);
  cogl_get_projection_matrix (&stage_projection);
  cogl_get_modelview_matrix (&stage_modelview);

  cogl_push_framebuffer (state->offscreen);

  cogl_set_viewport (stage_viewport[0],
                     stage_viewport[1],
                     stage_viewport[2],
                     stage_viewport[3]);
  cogl_set_projection_matrix (&stage_projection);
  cogl_set_modelview_matrix (&stage_modelview);

  paint_test_backface_culling (state);

  cogl_pop_framebuffer ();

  /* Incase we want feedback of what was drawn offscreen we draw it
   * to the stage... */
  cogl_set_source_texture (state->offscreen_tex);
  cogl_rectangle (0, TEXTURE_RENDER_SIZE * 16,
                  stage_viewport[2],
                  stage_viewport[3] + TEXTURE_RENDER_SIZE * 16);

  validate_result (0);
  validate_result (16);
}

static CoglHandle
make_texture (void)
{
  guchar *tex_data, *p;
  CoglHandle tex;

  tex_data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  for (p = tex_data + TEXTURE_SIZE * TEXTURE_SIZE * 4; p > tex_data;)
    {
      *(--p) = 255;
      *(--p) = 0;
      *(--p) = 0;
      *(--p) = 255;
    }

  tex = cogl_texture_new_from_data (TEXTURE_SIZE,
                                    TEXTURE_SIZE,
                                    COGL_TEXTURE_NO_ATLAS,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEXTURE_SIZE * 4,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

void
test_cogl_backface_culling (TestUtilsGTestFixture *fixture,
                            void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;
  CoglHandle tex;

  state.fb = shared_state->fb;
  state.width = cogl_framebuffer_get_width (shared_state->fb);
  state.height = cogl_framebuffer_get_height (shared_state->fb);

  state.offscreen = COGL_INVALID_HANDLE;

  state.texture = make_texture ();

  tex = cogl_texture_new_with_size (state.width, state.height,
                                    COGL_TEXTURE_NO_SLICING,
                                    COGL_PIXEL_FORMAT_ANY); /* internal fmt */
  state.offscreen = cogl_offscreen_new_to_texture (tex);
  state.offscreen_tex = tex;

  paint (&state);

  cogl_object_unref (state.offscreen);
  cogl_handle_unref (state.offscreen_tex);
  cogl_handle_unref (state.texture);

  if (g_test_verbose ())
    g_print ("OK\n");
}

