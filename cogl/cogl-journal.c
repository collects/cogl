/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-profile.h"
#include "cogl-attribute-private.h"
#include "cogl-point-in-poly-private.h"
#include "cogl-private.h"

#include <string.h>
#include <gmodule.h>
#include <math.h>

/* XXX NB:
 * The data logged in logged_vertices is formatted as follows:
 *
 * Per entry:
 *   4 RGBA GLubytes for the color
 *   2 floats for the top left position
 *   2 * n_layers floats for the top left texture coordinates
 *   2 floats for the bottom right position
 *   2 * n_layers floats for the bottom right texture coordinates
 */
#define GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS(N_LAYERS) \
  (N_LAYERS * 2 + 2)

/* XXX NB:
 * Once in the vertex array, the journal's vertex data is arranged as follows:
 * 4 vertices per quad:
 *    2 or 3 GLfloats per position (3 when doing software transforms)
 *    4 RGBA GLubytes,
 *    2 GLfloats per tex coord * n_layers
 *
 * Where n_layers corresponds to the number of pipeline layers enabled
 *
 * To avoid frequent changes in the stride of our vertex data we always pad
 * n_layers to be >= 2
 *
 * There will be four vertices per quad in the vertex array
 *
 * When we are transforming quads in software we need to also track the z
 * coordinate of transformed vertices.
 *
 * So for a given number of layers this gets the stride in 32bit words:
 */
#define SW_TRANSFORM      (!(COGL_DEBUG_ENABLED \
                             (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
#define POS_STRIDE        (SW_TRANSFORM ? 3 : 2) /* number of 32bit words */
#define N_POS_COMPONENTS  POS_STRIDE
#define COLOR_STRIDE      1 /* number of 32bit words */
#define TEX_STRIDE        2 /* number of 32bit words */
#define MIN_LAYER_PADING  2
#define GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS(N_LAYERS) \
  (POS_STRIDE + COLOR_STRIDE + \
   TEX_STRIDE * (N_LAYERS < MIN_LAYER_PADING ? MIN_LAYER_PADING : N_LAYERS))

/* If a batch is longer than this threshold then we'll assume it's not
   worth doing software clipping and it's cheaper to program the GPU
   to do the clip */
#define COGL_JOURNAL_HARDWARE_CLIP_THRESHOLD 8

typedef struct _CoglJournalFlushState
{
  CoglJournal         *journal;

  CoglFramebuffer     *framebuffer;

  CoglAttributeBuffer *attribute_buffer;
  GArray              *attributes;
  int                  current_attribute;

  gsize                stride;
  size_t               array_offset;
  GLuint               current_vertex;

  CoglIndices         *indices;
  gsize                indices_type_size;

  CoglMatrixStack     *modelview_stack;
  CoglMatrixStack     *projection_stack;

  CoglPipeline        *pipeline;
} CoglJournalFlushState;

typedef void (*CoglJournalBatchCallback) (CoglJournalEntry *start,
                                          int n_entries,
                                          void *data);
typedef gboolean (*CoglJournalBatchTest) (CoglJournalEntry *entry0,
                                          CoglJournalEntry *entry1);

static void _cogl_journal_free (CoglJournal *journal);

COGL_OBJECT_DEFINE (Journal, journal);

static void
_cogl_journal_free (CoglJournal *journal)
{
  int i;

  if (journal->entries)
    g_array_free (journal->entries, TRUE);
  if (journal->vertices)
    g_array_free (journal->vertices, TRUE);

  for (i = 0; i < COGL_JOURNAL_VBO_POOL_SIZE; i++)
    if (journal->vbo_pool[i])
      cogl_object_unref (journal->vbo_pool[i]);

  g_slice_free (CoglJournal, journal);
}

CoglJournal *
_cogl_journal_new (void)
{
  CoglJournal *journal = g_slice_new0 (CoglJournal);

  journal->entries = g_array_new (FALSE, FALSE, sizeof (CoglJournalEntry));
  journal->vertices = g_array_new (FALSE, FALSE, sizeof (float));

  return _cogl_journal_object_new (journal);
}

static void
_cogl_journal_dump_logged_quad (guint8 *data, int n_layers)
{
  gsize stride = GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS (n_layers);
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_print ("n_layers = %d; rgba=0x%02X%02X%02X%02X\n",
           n_layers, data[0], data[1], data[2], data[3]);

  data += 4;

  for (i = 0; i < 2; i++)
    {
      float *v = (float *)data + (i * stride);
      int j;

      g_print ("v%d: x = %f, y = %f", i, v[0], v[1]);

      for (j = 0; j < n_layers; j++)
        {
          float *t = v + 2 + TEX_STRIDE * j;
          g_print (", tx%d = %f, ty%d = %f", j, t[0], j, t[1]);
        }
      g_print ("\n");
    }
}

static void
_cogl_journal_dump_quad_vertices (guint8 *data, int n_layers)
{
  gsize stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers);
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_print ("n_layers = %d; stride = %d; pos stride = %d; color stride = %d; "
           "tex stride = %d; stride in bytes = %d\n",
           n_layers, (int)stride, POS_STRIDE, COLOR_STRIDE,
           TEX_STRIDE, (int)stride * 4);

  for (i = 0; i < 4; i++)
    {
      float *v = (float *)data + (i * stride);
      guint8 *c = data + (POS_STRIDE * 4) + (i * stride * 4);
      int j;

      if (G_UNLIKELY (COGL_DEBUG_ENABLED
                      (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
        g_print ("v%d: x = %f, y = %f, rgba=0x%02X%02X%02X%02X",
                 i, v[0], v[1], c[0], c[1], c[2], c[3]);
      else
        g_print ("v%d: x = %f, y = %f, z = %f, rgba=0x%02X%02X%02X%02X",
                 i, v[0], v[1], v[2], c[0], c[1], c[2], c[3]);
      for (j = 0; j < n_layers; j++)
        {
          float *t = v + POS_STRIDE + COLOR_STRIDE + TEX_STRIDE * j;
          g_print (", tx%d = %f, ty%d = %f", j, t[0], j, t[1]);
        }
      g_print ("\n");
    }
}

static void
_cogl_journal_dump_quad_batch (guint8 *data, int n_layers, int n_quads)
{
  gsize byte_stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers) * 4;
  int i;

  g_print ("_cogl_journal_dump_quad_batch: n_layers = %d, n_quads = %d\n",
           n_layers, n_quads);
  for (i = 0; i < n_quads; i++)
    _cogl_journal_dump_quad_vertices (data + byte_stride * 2 * i, n_layers);
}

static void
batch_and_call (CoglJournalEntry *entries,
                int n_entries,
                CoglJournalBatchTest can_batch_callback,
                CoglJournalBatchCallback batch_callback,
                void *data)
{
  int i;
  int batch_len = 1;
  CoglJournalEntry *batch_start = entries;

  if (n_entries < 1)
    return;

  for (i = 1; i < n_entries; i++)
    {
      CoglJournalEntry *entry0 = &entries[i - 1];
      CoglJournalEntry *entry1 = entry0 + 1;

      if (can_batch_callback (entry0, entry1))
        {
          batch_len++;
          continue;
        }

      batch_callback (batch_start, batch_len, data);

      batch_start = entry1;
      batch_len = 1;
    }

  /* The last batch... */
  batch_callback (batch_start, batch_len, data);
}

static void
_cogl_journal_flush_modelview_and_entries (CoglJournalEntry *batch_start,
                                           int               batch_len,
                                           void             *data)
{
  CoglJournalFlushState *state = data;
  CoglAttribute **attributes;
  CoglDrawFlags draw_flags = (COGL_DRAW_SKIP_JOURNAL_FLUSH |
                              COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                              COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH |
                              COGL_DRAW_SKIP_LEGACY_STATE);

  COGL_STATIC_TIMER (time_flush_modelview_and_entries,
                     "flush: pipeline+entries", /* parent */
                     "flush: modelview+entries",
                     "The time spent flushing modelview + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_modelview_and_entries);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_BATCHING)))
    g_print ("BATCHING:     modelview batch len = %d\n", batch_len);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
    {
      _cogl_matrix_stack_set (state->modelview_stack,
                              &batch_start->model_view);
      _cogl_context_set_current_modelview (ctx, state->modelview_stack);
    }

  attributes = (CoglAttribute **)state->attributes->data;

  if (!_cogl_pipeline_get_real_blend_enabled (state->pipeline))
    draw_flags |= COGL_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE;

#ifdef HAVE_COGL_GL
  if (ctx->driver == COGL_DRIVER_GL)
    {
      /* XXX: it's rather evil that we sneak in the GL_QUADS enum here... */
      _cogl_framebuffer_draw_attributes (state->framebuffer,
                                         state->pipeline,
                                         GL_QUADS,
                                         state->current_vertex, batch_len * 4,
                                         attributes,
                                         state->attributes->len,
                                         draw_flags);
    }
  else
#endif /* HAVE_COGL_GL */
    {
      if (batch_len > 1)
        {
          CoglVerticesMode mode = COGL_VERTICES_MODE_TRIANGLES;
          int first_vertex = state->current_vertex * 6 / 4;
          _cogl_framebuffer_draw_indexed_attributes (state->framebuffer,
                                                     state->pipeline,
                                                     mode,
                                                     first_vertex,
                                                     batch_len * 6,
                                                     state->indices,
                                                     attributes,
                                                     state->attributes->len,
                                                     draw_flags);
        }
      else
        {
          _cogl_framebuffer_draw_attributes (state->framebuffer,
                                             state->pipeline,
                                             COGL_VERTICES_MODE_TRIANGLE_FAN,
                                             state->current_vertex, 4,
                                             attributes,
                                             state->attributes->len,
                                             draw_flags);
        }
    }

  /* DEBUGGING CODE XXX: This path will cause all rectangles to be
   * drawn with a coloured outline. Each batch will be rendered with
   * the same color. This may e.g. help with debugging texture slicing
   * issues, visually seeing what is batched and debugging blending
   * issues, plus it looks quite cool.
   */
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_RECTANGLES)))
    {
      static CoglPipeline *outline = NULL;
      guint8 color_intensity;
      int i;
      CoglAttribute *loop_attributes[1];

      _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

      if (outline == NULL)
        outline = cogl_pipeline_new ();

      /* The least significant three bits represent the three
         components so that the order of colours goes red, green,
         yellow, blue, magenta, cyan. Black and white are skipped. The
         next two bits give four scales of intensity for those colours
         in the order 0xff, 0xcc, 0x99, and 0x66. This gives a total
         of 24 colours. If there are more than 24 batches on the stage
         then it will wrap around */
      color_intensity = 0xff - 0x33 * (ctxt->journal_rectangles_color >> 3);
      cogl_pipeline_set_color4ub (outline,
                                  (ctxt->journal_rectangles_color & 1) ?
                                  color_intensity : 0,
                                  (ctxt->journal_rectangles_color & 2) ?
                                  color_intensity : 0,
                                  (ctxt->journal_rectangles_color & 4) ?
                                  color_intensity : 0,
                                  0xff);

      loop_attributes[0] = attributes[0]; /* we just want the position */
      for (i = 0; i < batch_len; i++)
        _cogl_framebuffer_draw_attributes (state->framebuffer,
                                           outline,
                                           COGL_VERTICES_MODE_LINE_LOOP,
                                           4 * i + state->current_vertex, 4,
                                           loop_attributes,
                                           1,
                                           draw_flags);

      /* Go to the next color */
      do
        ctxt->journal_rectangles_color = ((ctxt->journal_rectangles_color + 1) &
                                          ((1 << 5) - 1));
      /* We don't want to use black or white */
      while ((ctxt->journal_rectangles_color & 0x07) == 0
             || (ctxt->journal_rectangles_color & 0x07) == 0x07);
    }

  state->current_vertex += (4 * batch_len);

  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_modelview_and_entries);
}

static gboolean
compare_entry_modelviews (CoglJournalEntry *entry0,
                          CoglJournalEntry *entry1)
{
  /* Batch together quads with the same model view matrix */

  /* FIXME: this is nasty, there are much nicer ways to track this
   * (at the add_quad_vertices level) without resorting to a memcmp!
   *
   * E.g. If the cogl-current-matrix code maintained an "age" for
   * the modelview matrix we could simply check in add_quad_vertices
   * if the age has increased, and if so record the change as a
   * boolean in the journal.
   */

  if (memcmp (&entry0->model_view, &entry1->model_view,
              sizeof (GLfloat) * 16) == 0)
    return TRUE;
  else
    return FALSE;
}

/* At this point we have a run of quads that we know have compatible
 * pipelines, but they may not all have the same modelview matrix */
static void
_cogl_journal_flush_pipeline_and_entries (CoglJournalEntry *batch_start,
                                          int               batch_len,
                                          void             *data)
{
  CoglJournalFlushState *state = data;
  COGL_STATIC_TIMER (time_flush_pipeline_entries,
                     "flush: texcoords+pipeline+entries", /* parent */
                     "flush: pipeline+entries",
                     "The time spent flushing pipeline + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_pipeline_entries);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_BATCHING)))
    g_print ("BATCHING:    pipeline batch len = %d\n", batch_len);

  state->pipeline = batch_start->pipeline;

  /* If we haven't transformed the quads in software then we need to also break
   * up batches according to changes in the modelview matrix... */
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
    {
      batch_and_call (batch_start,
                      batch_len,
                      compare_entry_modelviews,
                      _cogl_journal_flush_modelview_and_entries,
                      data);
    }
  else
    _cogl_journal_flush_modelview_and_entries (batch_start, batch_len, data);

  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_pipeline_entries);
}

static gboolean
compare_entry_pipelines (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  /* batch rectangles using compatible pipelines */

  if (_cogl_pipeline_equal (entry0->pipeline,
                            entry1->pipeline,
                            (COGL_PIPELINE_STATE_ALL &
                             ~COGL_PIPELINE_STATE_COLOR),
                            COGL_PIPELINE_LAYER_STATE_ALL,
                            0))
    return TRUE;
  else
    return FALSE;
}

/* Since the stride may not reflect the number of texture layers in use
 * (due to padding) we deal with texture coordinate offsets separately
 * from vertex and color offsets... */
static void
_cogl_journal_flush_texcoord_vbo_offsets_and_entries (
                                          CoglJournalEntry *batch_start,
                                          int               batch_len,
                                          void             *data)
{
  CoglJournalFlushState *state = data;
  int                    i;
  COGL_STATIC_TIMER (time_flush_texcoord_pipeline_entries,
                     "flush: vbo+texcoords+pipeline+entries", /* parent */
                     "flush: texcoords+pipeline+entries",
                     "The time spent flushing texcoord offsets + pipeline "
                     "+ entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_texcoord_pipeline_entries);

  /* NB: attributes 0 and 1 are position and color */

  for (i = 2; i < state->attributes->len; i++)
    cogl_object_unref (g_array_index (state->attributes, CoglAttribute *, i));

  g_array_set_size (state->attributes, batch_start->n_layers + 2);

  for (i = 0; i < batch_start->n_layers; i++)
    {
      CoglAttribute **attribute_entry =
        &g_array_index (state->attributes, CoglAttribute *, i + 2);
      const char *names[] = {
          "cogl_tex_coord0_in",
          "cogl_tex_coord1_in",
          "cogl_tex_coord2_in",
          "cogl_tex_coord3_in",
          "cogl_tex_coord4_in",
          "cogl_tex_coord5_in",
          "cogl_tex_coord6_in",
          "cogl_tex_coord7_in"
      };
      char *name;

      /* XXX NB:
       * Our journal's vertex data is arranged as follows:
       * 4 vertices per quad:
       *    2 or 3 floats per position (3 when doing software transforms)
       *    4 RGBA bytes,
       *    2 floats per tex coord * n_layers
       * (though n_layers may be padded; see definition of
       *  GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details)
       */
      name = i < 8 ? (char *)names[i] :
        g_strdup_printf ("cogl_tex_coord%d_in", i);

      /* XXX: it may be worth having some form of static initializer for
       * attributes... */
      *attribute_entry =
        cogl_attribute_new (state->attribute_buffer,
                            name,
                            state->stride,
                            state->array_offset +
                            (POS_STRIDE + COLOR_STRIDE) * 4 +
                            TEX_STRIDE * 4 * i,
                            2,
                            COGL_ATTRIBUTE_TYPE_FLOAT);

      if (i >= 8)
        g_free (name);
    }

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_pipelines,
                  _cogl_journal_flush_pipeline_and_entries,
                  data);
  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_texcoord_pipeline_entries);
}

static gboolean
compare_entry_n_layers (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  if (entry0->n_layers == entry1->n_layers)
    return TRUE;
  else
    return FALSE;
}

/* At this point we know the stride has changed from the previous batch
 * of journal entries */
static void
_cogl_journal_flush_vbo_offsets_and_entries (CoglJournalEntry *batch_start,
                                             int               batch_len,
                                             void             *data)
{
  CoglJournalFlushState   *state = data;
  CoglContext             *ctx = state->framebuffer->context;
  gsize                    stride;
  int                      i;
  CoglAttribute          **attribute_entry;
  COGL_STATIC_TIMER (time_flush_vbo_texcoord_pipeline_entries,
                     "flush: clip+vbo+texcoords+pipeline+entries", /* parent */
                     "flush: vbo+texcoords+pipeline+entries",
                     "The time spent flushing vbo + texcoord offsets + "
                     "pipeline + entries",
                     0 /* no application private data */);

  COGL_TIMER_START (_cogl_uprof_context,
                    time_flush_vbo_texcoord_pipeline_entries);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_BATCHING)))
    g_print ("BATCHING:   vbo offset batch len = %d\n", batch_len);

  /* XXX NB:
   * Our journal's vertex data is arranged as follows:
   * 4 vertices per quad:
   *    2 or 3 GLfloats per position (3 when doing software transforms)
   *    4 RGBA GLubytes,
   *    2 GLfloats per tex coord * n_layers
   * (though n_layers may be padded; see definition of
   *  GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details)
   */
  stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (batch_start->n_layers);
  stride *= sizeof (float);
  state->stride = stride;

  for (i = 0; i < state->attributes->len; i++)
    cogl_object_unref (g_array_index (state->attributes, CoglAttribute *, i));

  g_array_set_size (state->attributes, 2);

  attribute_entry = &g_array_index (state->attributes, CoglAttribute *, 0);
  *attribute_entry = cogl_attribute_new (state->attribute_buffer,
                                         "cogl_position_in",
                                         stride,
                                         state->array_offset,
                                         N_POS_COMPONENTS,
                                         COGL_ATTRIBUTE_TYPE_FLOAT);

  attribute_entry = &g_array_index (state->attributes, CoglAttribute *, 1);
  *attribute_entry =
    cogl_attribute_new (state->attribute_buffer,
                        "cogl_color_in",
                        stride,
                        state->array_offset + (POS_STRIDE * 4),
                        4,
                        COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

  if (ctx->driver != COGL_DRIVER_GL)
    state->indices = cogl_get_rectangle_indices (ctx, batch_len);

  /* We only create new Attributes when the stride within the
   * AttributeBuffer changes. (due to a change in the number of pipeline
   * layers) While the stride remains constant we walk forward through
   * the above AttributeBuffer using a vertex offset passed to
   * cogl_draw_attributes
   */
  state->current_vertex = 0;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_JOURNAL)))
    {
      guint8 *verts;

      /* Mapping a buffer for read is probably a really bad thing to
         do but this will only happen during debugging so it probably
         doesn't matter */
      verts = ((guint8 *)cogl_buffer_map (COGL_BUFFER (state->attribute_buffer),
                                          COGL_BUFFER_ACCESS_READ, 0) +
               state->array_offset);

      _cogl_journal_dump_quad_batch (verts,
                                     batch_start->n_layers,
                                     batch_len);

      cogl_buffer_unmap (COGL_BUFFER (state->attribute_buffer));
    }

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_n_layers,
                  _cogl_journal_flush_texcoord_vbo_offsets_and_entries,
                  data);

  /* progress forward through the VBO containing all our vertices */
  state->array_offset += (stride * 4 * batch_len);
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_JOURNAL)))
    g_print ("new vbo offset = %lu\n", (unsigned long)state->array_offset);

  COGL_TIMER_STOP (_cogl_uprof_context,
                   time_flush_vbo_texcoord_pipeline_entries);
}

static gboolean
compare_entry_strides (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  /* Currently the only thing that affects the stride for our vertex arrays
   * is the number of pipeline layers. We need to update our VBO offsets
   * whenever the stride changes. */
  /* TODO: We should be padding the n_layers == 1 case as if it were
   * n_layers == 2 so we can reduce the need to split batches. */
  if (entry0->n_layers == entry1->n_layers ||
      (entry0->n_layers <= MIN_LAYER_PADING &&
       entry1->n_layers <= MIN_LAYER_PADING))
    return TRUE;
  else
    return FALSE;
}

/* At this point we know the batch has a unique clip stack */
static void
_cogl_journal_flush_clip_stacks_and_entries (CoglJournalEntry *batch_start,
                                             int               batch_len,
                                             void             *data)
{
  CoglJournalFlushState *state = data;

  COGL_STATIC_TIMER (time_flush_clip_stack_pipeline_entries,
                     "Journal Flush", /* parent */
                     "flush: clip+vbo+texcoords+pipeline+entries",
                     "The time spent flushing clip + vbo + texcoord offsets + "
                     "pipeline + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context,
                    time_flush_clip_stack_pipeline_entries);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_BATCHING)))
    g_print ("BATCHING:  clip stack batch len = %d\n", batch_len);

  _cogl_clip_stack_flush (batch_start->clip_stack, state->framebuffer);

  /* XXX: Because we are manually flushing clip state here we need to
   * make sure that the clip state gets updated the next time we flush
   * framebuffer state by marking the current framebuffer's clip state
   * as changed. */
  ctx->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;

  _cogl_matrix_stack_push (state->modelview_stack);

  /* If we have transformed all our quads at log time then we ensure
   * no further model transform is applied by loading the identity
   * matrix here. We need to do this after flushing the clip stack
   * because the clip stack flushing code can modify the matrix */
  if (G_LIKELY (!(COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))))
    {
      _cogl_matrix_stack_load_identity (state->modelview_stack);
      _cogl_context_set_current_modelview (ctx, state->modelview_stack);
    }

  /* Setting up the clip state can sometimes also flush the projection
     matrix so we should flush it again. This will be a no-op if the
     clip code didn't modify the projection */
  _cogl_context_set_current_projection (ctx, state->projection_stack);

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_strides,
                  _cogl_journal_flush_vbo_offsets_and_entries, /* callback */
                  data);

  _cogl_matrix_stack_pop (state->modelview_stack);

  COGL_TIMER_STOP (_cogl_uprof_context,
                   time_flush_clip_stack_pipeline_entries);
}

static gboolean
calculate_translation (const CoglMatrix *a,
                       const CoglMatrix *b,
                       float *tx_p,
                       float *ty_p)
{
  float tx, ty;
  int x, y;

  /* Assuming we had the original matrix in this form:
   *
   *      [ a₁₁, a₁₂, a₁₃, a₁₄ ]
   *      [ a₂₁, a₂₂, a₂₃, a₂₄ ]
   *  a = [ a₃₁, a₃₂, a₃₃, a₃₄ ]
   *      [ a₄₁, a₄₂, a₄₃, a₄₄ ]
   *
   * then a translation of that matrix would be a multiplication by a
   * matrix of this form:
   *
   *      [ 1, 0, 0, x ]
   *      [ 0, 1, 0, y ]
   *  t = [ 0, 0, 1, 0 ]
   *      [ 0, 0, 0, 1 ]
   *
   * That would give us a matrix of this form.
   *
   *              [ a₁₁, a₁₂, a₁₃, a₁₁ x + a₁₂ y + a₁₄ ]
   *              [ a₂₁, a₂₂, a₂₃, a₂₁ x + a₂₂ y + a₂₄ ]
   *  b = a ⋅ t = [ a₃₁, a₃₂, a₃₃, a₃₁ x + a₃₂ y + a₃₄ ]
   *              [ a₄₁, a₄₂, a₄₃, a₄₁ x + a₄₂ y + a₄₄ ]
   *
   * We can use the two equations from the top left of the matrix to
   * work out the x and y translation given the two matrices:
   *
   *  b₁₄ = a₁₁x + a₁₂y + a₁₄
   *  b₂₄ = a₂₁x + a₂₂y + a₂₄
   *
   * Rearranging gives us:
   *
   *        a₁₂ b₂₄ - a₂₄ a₁₂
   *        -----------------  +  a₁₄ - b₁₄
   *              a₂₂
   *  x =  ---------------------------------
   *                a₁₂ a₂₁
   *                -------  -  a₁₁
   *                  a₂₂
   *
   *      b₂₄ - a₂₁x - a₂₄
   *  y = ----------------
   *            a₂₂
   *
   * Once we've worked out what x and y would be if this was a valid
   * translation then we can simply verify that the rest of the matrix
   * matches up.
   */

  /* The leftmost 3x4 part of the matrix shouldn't change by a
     translation so we can just compare it directly */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 3; x++)
      if ((&a->xx)[x * 4 + y] != (&b->xx)[x * 4 + y])
        return FALSE;

  tx = (((a->xy * b->yw - a->yw * a->xy) / a->yy + a->xw - b->xw) /
        ((a->xy * a->yx) / a->yy - a->xx));
  ty = (b->yw - a->yx * tx - a->yw) / a->yy;

#define APPROX_EQUAL(a, b) (fabsf ((a) - (b)) < 1e-6f)

  /* Check whether the 4th column of the matrices match up to the
     calculation */
  if (!APPROX_EQUAL (b->xw, a->xx * tx + a->xy * ty + a->xw) ||
      !APPROX_EQUAL (b->yw, a->yx * tx + a->yy * ty + a->yw) ||
      !APPROX_EQUAL (b->zw, a->zx * tx + a->zy * ty + a->zw) ||
      !APPROX_EQUAL (b->ww, a->wx * tx + a->wy * ty + a->ww))
    return FALSE;

#undef APPROX_EQUAL

  *tx_p = tx;
  *ty_p = ty;

  return TRUE;
}

typedef struct
{
  float x_1, y_1;
  float x_2, y_2;
} ClipBounds;

static gboolean
can_software_clip_entry (CoglJournalEntry *journal_entry,
                         CoglJournalEntry *prev_journal_entry,
                         CoglClipStack *clip_stack,
                         ClipBounds *clip_bounds_out)
{
  CoglPipeline *pipeline = journal_entry->pipeline;
  CoglClipStack *clip_entry;
  int layer_num;

  clip_bounds_out->x_1 = -G_MAXFLOAT;
  clip_bounds_out->y_1 = -G_MAXFLOAT;
  clip_bounds_out->x_2 = G_MAXFLOAT;
  clip_bounds_out->y_2 = G_MAXFLOAT;

  /* Check the pipeline is usable. We can short-cut here for
     entries using the same pipeline as the previous entry */
  if (prev_journal_entry == NULL || pipeline != prev_journal_entry->pipeline)
    {
      /* If the pipeline has a user program then we can't reliably modify
         the texture coordinates */
      if (cogl_pipeline_get_user_program (pipeline))
        return FALSE;

      /* If any of the pipeline layers have a texture matrix then we can't
         reliably modify the texture coordinates */
      for (layer_num = cogl_pipeline_get_n_layers (pipeline) - 1;
           layer_num >= 0;
           layer_num--)
        if (_cogl_pipeline_layer_has_user_matrix (pipeline, layer_num))
          return FALSE;
    }

  /* Now we need to verify that each clip entry's matrix is just a
     translation of the journal entry's modelview matrix. We can
     also work out the bounds of the clip in modelview space using
     this translation */
  for (clip_entry = clip_stack; clip_entry; clip_entry = clip_entry->parent)
    {
      float rect_x1, rect_y1, rect_x2, rect_y2;
      CoglClipStackRect *clip_rect;
      float tx, ty;

      clip_rect = (CoglClipStackRect *) clip_entry;

      if (!calculate_translation (&clip_rect->matrix,
                                  &journal_entry->model_view,
                                  &tx, &ty))
        return FALSE;

      if (clip_rect->x0 < clip_rect->x1)
        {
          rect_x1 = clip_rect->x0;
          rect_x2 = clip_rect->x1;
        }
      else
        {
          rect_x1 = clip_rect->x1;
          rect_x2 = clip_rect->x0;
        }
      if (clip_rect->y0 < clip_rect->y1)
        {
          rect_y1 = clip_rect->y0;
          rect_y2 = clip_rect->y1;
        }
      else
        {
          rect_y1 = clip_rect->y1;
          rect_y2 = clip_rect->y0;
        }

      clip_bounds_out->x_1 = MAX (clip_bounds_out->x_1, rect_x1 - tx);
      clip_bounds_out->y_1 = MAX (clip_bounds_out->y_1, rect_y1 - ty);
      clip_bounds_out->x_2 = MIN (clip_bounds_out->x_2, rect_x2 - tx);
      clip_bounds_out->y_2 = MIN (clip_bounds_out->y_2, rect_y2 - ty);
    }

  if (clip_bounds_out->x_2 <= clip_bounds_out->x_1 ||
      clip_bounds_out->y_2 <= clip_bounds_out->y_1)
    memset (clip_bounds_out, 0, sizeof (ClipBounds));

  return TRUE;
}

static void
software_clip_entry (CoglJournalEntry *journal_entry,
                     float *verts,
                     ClipBounds *clip_bounds)
{
  size_t stride =
    GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS (journal_entry->n_layers);
  float rx1, ry1, rx2, ry2;
  float vx1, vy1, vx2, vy2;
  int layer_num;

  /* Remove the clip on the entry */
  _cogl_clip_stack_unref (journal_entry->clip_stack);
  journal_entry->clip_stack = NULL;

  vx1 = verts[0];
  vy1 = verts[1];
  vx2 = verts[stride];
  vy2 = verts[stride + 1];

  if (vx1 < vx2)
    {
      rx1 = vx1;
      rx2 = vx2;
    }
  else
    {
      rx1 = vx2;
      rx2 = vx1;
    }
  if (vy1 < vy2)
    {
      ry1 = vy1;
      ry2 = vy2;
    }
  else
    {
      ry1 = vy2;
      ry2 = vy1;
    }

  rx1 = CLAMP (rx1, clip_bounds->x_1, clip_bounds->x_2);
  ry1 = CLAMP (ry1, clip_bounds->y_1, clip_bounds->y_2);
  rx2 = CLAMP (rx2, clip_bounds->x_1, clip_bounds->x_2);
  ry2 = CLAMP (ry2, clip_bounds->y_1, clip_bounds->y_2);

  /* Check if the rectangle intersects the clip at all */
  if (rx1 == rx2 || ry1 == ry2)
    /* Will set all of the vertex data to 0 in the hope that this
       will create a degenerate rectangle and the GL driver will
       be able to clip it quickly */
    memset (verts, 0, sizeof (float) * stride * 2);
  else
    {
      if (vx1 > vx2)
        {
          float t = rx1;
          rx1 = rx2;
          rx2 = t;
        }
      if (vy1 > vy2)
        {
          float t = ry1;
          ry1 = ry2;
          ry2 = t;
        }

      verts[0] = rx1;
      verts[1] = ry1;
      verts[stride] = rx2;
      verts[stride + 1] = ry2;

      /* Convert the rectangle coordinates to a fraction of the original
         rectangle */
      rx1 = (rx1 - vx1) / (vx2 - vx1);
      ry1 = (ry1 - vy1) / (vy2 - vy1);
      rx2 = (rx2 - vx1) / (vx2 - vx1);
      ry2 = (ry2 - vy1) / (vy2 - vy1);

      for (layer_num = 0; layer_num < journal_entry->n_layers; layer_num++)
        {
          float *t = verts + 2 + 2 * layer_num;
          float tx1 = t[0], ty1 = t[1];
          float tx2 = t[stride], ty2 = t[stride + 1];
          t[0] = rx1 * (tx2 - tx1) + tx1;
          t[1] = ry1 * (ty2 - ty1) + ty1;
          t[stride] = rx2 * (tx2 - tx1) + tx1;
          t[stride + 1] = ry2 * (ty2 - ty1) + ty1;
        }
    }
}

static void
maybe_software_clip_entries (CoglJournalEntry      *batch_start,
                             int                    batch_len,
                             CoglJournalFlushState *state)
{
  CoglJournal *journal = state->journal;
  CoglClipStack *clip_stack, *clip_entry;
  int entry_num;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* This tries to find cases where the entry is logged with a clip
     but it would be faster to modify the vertex and texture
     coordinates rather than flush the clip so that it can batch
     better */

  /* If the batch is reasonably long then it's worthwhile programming
     the GPU to do the clip */
  if (batch_len >= COGL_JOURNAL_HARDWARE_CLIP_THRESHOLD)
    return;

  clip_stack = batch_start->clip_stack;

  if (clip_stack == NULL)
    return;

  /* Verify that all of the clip stack entries are a simple rectangle
     clip */
  for (clip_entry = clip_stack; clip_entry; clip_entry = clip_entry->parent)
    if (clip_entry->type != COGL_CLIP_STACK_RECT)
      return;

  /* This scratch buffer is used to store the translation for each
     entry in the journal. We store it in a separate buffer because
     it's expensive to calculate but at this point we still don't know
     whether we can clip all of the entries so we don't want to do the
     rest of the dependant calculations until we're sure we can. */
  if (ctx->journal_clip_bounds == NULL)
    ctx->journal_clip_bounds = g_array_new (FALSE, FALSE, sizeof (ClipBounds));
  g_array_set_size (ctx->journal_clip_bounds, batch_len);

  for (entry_num = 0; entry_num < batch_len; entry_num++)
    {
      CoglJournalEntry *journal_entry = batch_start + entry_num;
      CoglJournalEntry *prev_journal_entry =
        entry_num ? batch_start + (entry_num - 1) : NULL;
      ClipBounds *clip_bounds = &g_array_index (ctx->journal_clip_bounds,
                                                ClipBounds, entry_num);

      if (!can_software_clip_entry (journal_entry, prev_journal_entry,
                                    clip_stack,
                                    clip_bounds))
        return;
    }

  /* If we make it here then we know we can software clip the entire batch */

  COGL_NOTE (CLIPPING, "Software clipping a batch of length %i", batch_len);

  for (entry_num = 0; entry_num < batch_len; entry_num++)
    {
      CoglJournalEntry *journal_entry = batch_start + entry_num;
      float *verts = &g_array_index (journal->vertices, float,
                                     journal_entry->array_offset + 1);
      ClipBounds *clip_bounds = &g_array_index (ctx->journal_clip_bounds,
                                                ClipBounds, entry_num);

      software_clip_entry (journal_entry, verts, clip_bounds);
    }

  return;
}

static void
_cogl_journal_maybe_software_clip_entries (CoglJournalEntry *batch_start,
                                           int               batch_len,
                                           void             *data)
{
  CoglJournalFlushState *state = data;

  COGL_STATIC_TIMER (time_check_software_clip,
                     "Journal Flush", /* parent */
                     "flush: software clipping",
                     "Time spent software clipping",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context,
                    time_check_software_clip);

  maybe_software_clip_entries (batch_start, batch_len, state);

  COGL_TIMER_STOP (_cogl_uprof_context,
                   time_check_software_clip);
}

static gboolean
compare_entry_clip_stacks (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  return entry0->clip_stack == entry1->clip_stack;
}

/* Gets a new vertex array from the pool. A reference is taken on the
   array so it can be treated as if it was just newly allocated */
static CoglAttributeBuffer *
create_attribute_buffer (CoglJournal *journal,
                         gsize n_bytes)
{
  CoglAttributeBuffer *vbo;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* If CoglBuffers are being emulated with malloc then there's not
     really any point in using the pool so we'll just allocate the
     buffer directly */
  if (!(ctx->private_feature_flags & COGL_PRIVATE_FEATURE_VBOS))
    return cogl_attribute_buffer_new (ctx, n_bytes, NULL);

  vbo = journal->vbo_pool[journal->next_vbo_in_pool];

  if (vbo == NULL)
    {
      vbo = cogl_attribute_buffer_new (ctx, n_bytes, NULL);
      journal->vbo_pool[journal->next_vbo_in_pool] = vbo;
    }
  else if (cogl_buffer_get_size (COGL_BUFFER (vbo)) < n_bytes)
    {
      /* If the buffer is too small then we'll just recreate it */
      cogl_object_unref (vbo);
      vbo = cogl_attribute_buffer_new (ctx, n_bytes, NULL);
      journal->vbo_pool[journal->next_vbo_in_pool] = vbo;
    }

  journal->next_vbo_in_pool = ((journal->next_vbo_in_pool + 1) %
                               COGL_JOURNAL_VBO_POOL_SIZE);

  return cogl_object_ref (vbo);
}

static CoglAttributeBuffer *
upload_vertices (CoglJournal            *journal,
                 const CoglJournalEntry *entries,
                 int                     n_entries,
                 size_t                  needed_vbo_len,
                 GArray                 *vertices)
{
  CoglAttributeBuffer *attribute_buffer;
  CoglBuffer *buffer;
  const float *vin;
  float *vout;
  int entry_num;
  int i;

  g_assert (needed_vbo_len);

  attribute_buffer = create_attribute_buffer (journal, needed_vbo_len * 4);
  buffer = COGL_BUFFER (attribute_buffer);
  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_STATIC);

  vout = _cogl_buffer_map_for_fill_or_fallback (buffer);
  vin = &g_array_index (vertices, float, 0);

  /* Expand the number of vertices from 2 to 4 while uploading */
  for (entry_num = 0; entry_num < n_entries; entry_num++)
    {
      const CoglJournalEntry *entry = entries + entry_num;
      size_t vb_stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (entry->n_layers);
      size_t array_stride =
        GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS (entry->n_layers);

      /* Copy the color to all four of the vertices */
      for (i = 0; i < 4; i++)
        memcpy (vout + vb_stride * i + POS_STRIDE, vin, 4);
      vin++;

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
        {
          vout[vb_stride * 0] = vin[0];
          vout[vb_stride * 0 + 1] = vin[1];
          vout[vb_stride * 1] = vin[0];
          vout[vb_stride * 1 + 1] = vin[array_stride + 1];
          vout[vb_stride * 2] = vin[array_stride];
          vout[vb_stride * 2 + 1] = vin[array_stride + 1];
          vout[vb_stride * 3] = vin[array_stride];
          vout[vb_stride * 3 + 1] = vin[1];
        }
      else
        {
          float v[8];

          v[0] = vin[0];
          v[1] = vin[1];
          v[2] = vin[0];
          v[3] = vin[array_stride + 1];
          v[4] = vin[array_stride];
          v[5] = vin[array_stride + 1];
          v[6] = vin[array_stride];
          v[7] = vin[1];

          cogl_matrix_transform_points (&entry->model_view,
                                        2, /* n_components */
                                        sizeof (float) * 2, /* stride_in */
                                        v, /* points_in */
                                        /* strideout */
                                        vb_stride * sizeof (float),
                                        vout, /* points_out */
                                        4 /* n_points */);
        }

      for (i = 0; i < entry->n_layers; i++)
        {
          const float *tin = vin + 2;
          float *tout = vout + POS_STRIDE + COLOR_STRIDE;

          tout[vb_stride * 0 + i * 2] = tin[i * 2];
          tout[vb_stride * 0 + 1 + i * 2] = tin[i * 2 + 1];
          tout[vb_stride * 1 + i * 2] = tin[i * 2];
          tout[vb_stride * 1 + 1 + i * 2] = tin[array_stride + i * 2 + 1];
          tout[vb_stride * 2 + i * 2] = tin[array_stride + i * 2];
          tout[vb_stride * 2 + 1 + i * 2] = tin[array_stride + i * 2 + 1];
          tout[vb_stride * 3 + i * 2] = tin[array_stride + i * 2];
          tout[vb_stride * 3 + 1 + i * 2] = tin[i * 2 + 1];
        }

      vin += array_stride * 2;
      vout += vb_stride * 4;
    }

  _cogl_buffer_unmap_for_fill_or_fallback (buffer);

  return attribute_buffer;
}

void
_cogl_journal_discard (CoglJournal *journal)
{
  int i;

  for (i = 0; i < journal->entries->len; i++)
    {
      CoglJournalEntry *entry =
        &g_array_index (journal->entries, CoglJournalEntry, i);
      _cogl_pipeline_journal_unref (entry->pipeline);
      _cogl_clip_stack_unref (entry->clip_stack);
    }

  g_array_set_size (journal->entries, 0);
  g_array_set_size (journal->vertices, 0);
  journal->needed_vbo_len = 0;
  journal->fast_read_pixel_count = 0;

  /* The journal only holds a reference to the framebuffer while the
     journal is not empty */
  if (journal->framebuffer)
    {
      cogl_object_unref (journal->framebuffer);
      journal->framebuffer = NULL;
    }
}

/* Note: A return value of FALSE doesn't mean 'no' it means
 * 'unknown' */
gboolean
_cogl_journal_all_entries_within_bounds (CoglJournal *journal,
                                         float clip_x0,
                                         float clip_y0,
                                         float clip_x1,
                                         float clip_y1)
{
  CoglJournalEntry *entry = (CoglJournalEntry *)journal->entries->data;
  CoglClipStack *clip_entry;
  CoglClipStack *reference = NULL;
  int bounds_x0;
  int bounds_y0;
  int bounds_x1;
  int bounds_y1;
  int i;

  if (journal->entries->len == 0)
    return TRUE;

  /* Find the shortest clip_stack ancestry that leaves us in the
   * required bounds */
  for (clip_entry = entry->clip_stack;
       clip_entry;
       clip_entry = clip_entry->parent)
    {
      _cogl_clip_stack_get_bounds (clip_entry,
                                   &bounds_x0, &bounds_y0,
                                   &bounds_x1, &bounds_y1);

      if (bounds_x0 >= clip_x0 && bounds_y0 >= clip_y0 &&
          bounds_x1 <= clip_x1 && bounds_y1 <= clip_y1)
        reference = clip_entry;
      else
        break;
    }

  if (!reference)
    return FALSE;

  /* For the remaining journal entries we will only verify they share
   * 'reference' as an ancestor in their clip stack since that's
   * enough to know that they would be within the required bounds.
   */
  for (i = 1; i < journal->entries->len; i++)
    {
      gboolean found_reference = FALSE;
      entry = &g_array_index (journal->entries, CoglJournalEntry, i);

      for (clip_entry = entry->clip_stack;
           clip_entry;
           clip_entry = clip_entry->parent)
        {
          if (clip_entry == reference)
            {
              found_reference = TRUE;
              break;
            }
        }

      if (!found_reference)
        return FALSE;
    }

  return TRUE;
}

/* XXX NB: When _cogl_journal_flush() returns all state relating
 * to pipelines, all glEnable flags and current matrix state
 * is undefined.
 */
void
_cogl_journal_flush (CoglJournal *journal,
                     CoglFramebuffer *framebuffer)
{
  CoglJournalFlushState state;
  int                   i;
  CoglMatrixStack      *modelview_stack;
  COGL_STATIC_TIMER (flush_timer,
                     "Mainloop", /* parent */
                     "Journal Flush",
                     "The time spent flushing the Cogl journal",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (journal->entries->len == 0)
    return;

  /* Something has gone wrong if we're using a different framebuffer
     to flush than the one that was current when the entries were
     added */
  _COGL_RETURN_IF_FAIL (framebuffer == journal->framebuffer);

  /* The entries in this journal may depend on images in other
   * framebuffers which may require that we flush the journals
   * associated with those framebuffers before we can flush
   * this journal... */
  _cogl_framebuffer_flush_dependency_journals (framebuffer);

  /* Note: we start the timer after flushing dependency journals so
   * that the timer isn't started recursively. */
  COGL_TIMER_START (_cogl_uprof_context, flush_timer);

  state.framebuffer = framebuffer;
  cogl_push_framebuffer (framebuffer);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_BATCHING)))
    g_print ("BATCHING: journal len = %d\n", journal->entries->len);

  /* NB: the journal deals with flushing the modelview stack and clip
     state manually */
  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_ALL &
                                 ~(COGL_FRAMEBUFFER_STATE_MODELVIEW |
                                   COGL_FRAMEBUFFER_STATE_CLIP));

  state.journal = journal;

  state.attributes = ctx->journal_flush_attributes_array;

  modelview_stack = _cogl_framebuffer_get_modelview_stack (framebuffer);
  state.modelview_stack = modelview_stack;
  state.projection_stack = _cogl_framebuffer_get_projection_stack (framebuffer);

  if (G_UNLIKELY ((COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_SOFTWARE_CLIP)) == 0))
    {
      /* We do an initial walk of the journal to analyse the clip stack
         batches to see if we can do software clipping. We do this as a
         separate walk of the journal because we can modify entries and
         this may end up joining together clip stack batches in the next
         iteration. */
      batch_and_call ((CoglJournalEntry *)journal->entries->data, /* first entry */
                      journal->entries->len, /* max number of entries to consider */
                      compare_entry_clip_stacks,
                      _cogl_journal_maybe_software_clip_entries, /* callback */
                      &state); /* data */
    }

  /* We upload the vertices after the clip stack pass in case it
     modifies the entries */
  state.attribute_buffer =
    upload_vertices (journal,
                     &g_array_index (journal->entries, CoglJournalEntry, 0),
                     journal->entries->len,
                     journal->needed_vbo_len,
                     journal->vertices);
  state.array_offset = 0;

  /* batch_and_call() batches a list of journal entries according to some
   * given criteria and calls a callback once for each determined batch.
   *
   * The process of flushing the journal is staggered to reduce the amount
   * of driver/GPU state changes necessary:
   * 1) We split the entries according to the clip state.
   * 2) We split the entries according to the stride of the vertices:
   *      Each time the stride of our vertex data changes we need to call
   *      gl{Vertex,Color}Pointer to inform GL of new VBO offsets.
   *      Currently the only thing that affects the stride of our vertex data
   *      is the number of pipeline layers.
   * 3) We split the entries explicitly by the number of pipeline layers:
   *      We pad our vertex data when the number of layers is < 2 so that we
   *      can minimize changes in stride. Each time the number of layers
   *      changes we need to call glTexCoordPointer to inform GL of new VBO
   *      offsets.
   * 4) We then split according to compatible Cogl pipelines:
   *      This is where we flush pipeline state
   * 5) Finally we split according to modelview matrix changes:
   *      This is when we finally tell GL to draw something.
   *      Note: Splitting by modelview changes is skipped when are doing the
   *      vertex transformation in software at log time.
   */
  batch_and_call ((CoglJournalEntry *)journal->entries->data, /* first entry */
                  journal->entries->len, /* max number of entries to consider */
                  compare_entry_clip_stacks,
                  _cogl_journal_flush_clip_stacks_and_entries, /* callback */
                  &state); /* data */

  for (i = 0; i < state.attributes->len; i++)
    cogl_object_unref (g_array_index (state.attributes, CoglAttribute *, i));
  g_array_set_size (state.attributes, 0);

  cogl_object_unref (state.attribute_buffer);

  _cogl_journal_discard (journal);

  cogl_pop_framebuffer ();

  COGL_TIMER_STOP (_cogl_uprof_context, flush_timer);
}

static gboolean
add_framebuffer_deps_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglFramebuffer *framebuffer = user_data;
  CoglTexture *texture = _cogl_pipeline_layer_get_texture_real (layer);
  const GList *l;

  if (!texture)
    return TRUE;

  for (l = _cogl_texture_get_associated_framebuffers (texture); l; l = l->next)
    _cogl_framebuffer_add_dependency (framebuffer, l->data);

  return TRUE;
}

void
_cogl_journal_log_quad (CoglJournal  *journal,
                        const float  *position,
                        CoglPipeline *pipeline,
                        int           n_layers,
                        CoglTexture  *layer0_override_texture,
                        const float  *tex_coords,
                        unsigned int  tex_coords_len)
{
  gsize            stride;
  int               next_vert;
  float            *v;
  int               i;
  int               next_entry;
  guint32           disable_layers;
  CoglJournalEntry *entry;
  CoglPipeline     *final_pipeline;
  CoglClipStack    *clip_stack;
  CoglPipelineFlushOptions flush_options;
  COGL_STATIC_TIMER (log_timer,
                     "Mainloop", /* parent */
                     "Journal Log",
                     "The time spent logging in the Cogl journal",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, log_timer);

  /* If the framebuffer was previously empty then we'll take a
     reference to the current framebuffer. This reference will be
     removed when the journal is flushed. FIXME: This should probably
     be being passed a pointer to the framebuffer from further up so
     that we don't have to rely on the global framebuffer stack */
  if (journal->vertices->len == 0)
    journal->framebuffer = cogl_object_ref (cogl_get_draw_framebuffer ());

  /* The vertex data is logged into a separate array. The data needs
     to be copied into a vertex array before it's given to GL so we
     only store two vertices per quad and expand it to four while
     uploading. */

  /* XXX: See definition of GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS for details
   * about how we pack our vertex data */
  stride = GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS (n_layers);

  next_vert = journal->vertices->len;
  g_array_set_size (journal->vertices, next_vert + 2 * stride + 1);
  v = &g_array_index (journal->vertices, float, next_vert);

  /* We calculate the needed size of the vbo as we go because it
     depends on the number of layers in each entry and it's not easy
     calculate based on the length of the logged vertices array */
  journal->needed_vbo_len += GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers) * 4;

  /* XXX: All the jumping around to fill in this strided buffer doesn't
   * seem ideal. */

  /* FIXME: This is a hacky optimization, since it will break if we
   * change the definition of CoglColor: */
  _cogl_pipeline_get_colorubv (pipeline, (guint8 *) v);
  v++;

  memcpy (v, position, sizeof (float) * 2);
  memcpy (v + stride, position + 2, sizeof (float) * 2);

  for (i = 0; i < n_layers; i++)
    {
      /* XXX: See definition of GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS
       * for details about how we pack our vertex data */
      GLfloat *t = v + 2 + i * 2;

      memcpy (t, tex_coords + i * 4, sizeof (float) * 2);
      memcpy (t + stride, tex_coords + i * 4 + 2, sizeof (float) * 2);
    }

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_JOURNAL)))
    {
      g_print ("Logged new quad:\n");
      v = &g_array_index (journal->vertices, float, next_vert);
      _cogl_journal_dump_logged_quad ((guint8 *)v, n_layers);
    }

  next_entry = journal->entries->len;
  g_array_set_size (journal->entries, next_entry + 1);
  entry = &g_array_index (journal->entries, CoglJournalEntry, next_entry);

  entry->n_layers = n_layers;
  entry->array_offset = next_vert;

  final_pipeline = pipeline;

  flush_options.flags = 0;
  if (G_UNLIKELY (cogl_pipeline_get_n_layers (pipeline) != n_layers))
    {
      disable_layers = (1 << n_layers) - 1;
      disable_layers = ~disable_layers;
      flush_options.disable_layers = disable_layers;
      flush_options.flags |= COGL_PIPELINE_FLUSH_DISABLE_MASK;
    }
  if (G_UNLIKELY (layer0_override_texture))
    {
      flush_options.flags |= COGL_PIPELINE_FLUSH_LAYER0_OVERRIDE;
      flush_options.layer0_override_texture = layer0_override_texture;
    }

  if (G_UNLIKELY (flush_options.flags))
    {
      final_pipeline = cogl_pipeline_copy (pipeline);
      _cogl_pipeline_apply_overrides (final_pipeline, &flush_options);
    }

  entry->pipeline = _cogl_pipeline_journal_ref (final_pipeline);

  clip_stack = _cogl_framebuffer_get_clip_stack (journal->framebuffer);
  entry->clip_stack = _cogl_clip_stack_ref (clip_stack);

  if (G_UNLIKELY (final_pipeline != pipeline))
    cogl_handle_unref (final_pipeline);

  cogl_get_modelview_matrix (&entry->model_view);

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         add_framebuffer_deps_cb,
                                         journal->framebuffer);

  /* XXX: It doesn't feel very nice that in this case we just assume
   * that the journal is associated with the current framebuffer. I
   * think a journal->framebuffer reference would seem nicer here but
   * the reason we don't have that currently is that it would
   * introduce a circular reference. */
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_BATCHING)))
    _cogl_framebuffer_flush_journal (journal->framebuffer);

  COGL_TIMER_STOP (_cogl_uprof_context, log_timer);
}

static void
entry_to_screen_polygon (CoglFramebuffer *framebuffer,
                         const CoglJournalEntry *entry,
                         float *vertices,
                         float *poly)
{
  size_t array_stride =
    GET_JOURNAL_ARRAY_STRIDE_FOR_N_LAYERS (entry->n_layers);
  CoglMatrixStack *projection_stack;
  CoglMatrix projection;
  int i;
  float viewport[4];

  poly[0] = vertices[0];
  poly[1] = vertices[1];
  poly[2] = 0;
  poly[3] = 1;

  poly[4] = vertices[0];
  poly[5] = vertices[array_stride + 1];
  poly[6] = 0;
  poly[7] = 1;

  poly[8] = vertices[array_stride];
  poly[9] = vertices[array_stride + 1];
  poly[10] = 0;
  poly[11] = 1;

  poly[12] = vertices[array_stride];
  poly[13] = vertices[1];
  poly[14] = 0;
  poly[15] = 1;

  /* TODO: perhaps split the following out into a more generalized
   * _cogl_transform_points utility...
   */

  cogl_matrix_transform_points (&entry->model_view,
                                2, /* n_components */
                                sizeof (float) * 4, /* stride_in */
                                poly, /* points_in */
                                /* strideout */
                                sizeof (float) * 4,
                                poly, /* points_out */
                                4 /* n_points */);

  projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  _cogl_matrix_stack_get (projection_stack, &projection);

  cogl_matrix_project_points (&projection,
                              3, /* n_components */
                              sizeof (float) * 4, /* stride_in */
                              poly, /* points_in */
                              /* strideout */
                              sizeof (float) * 4,
                              poly, /* points_out */
                              4 /* n_points */);

  cogl_framebuffer_get_viewport4fv (framebuffer, viewport);

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

  /* Scale from normalized device coordinates (in range [-1,1]) to
   * window coordinates ranging [0,window-size] ... */
  for (i = 0; i < 4; i++)
    {
      float w = poly[4 * i + 3];

      /* Perform perspective division */
      poly[4 * i] /= w;
      poly[4 * i + 1] /= w;

      /* Apply viewport transform */
      poly[4 * i] = VIEWPORT_TRANSFORM_X (poly[4 * i],
                                          viewport[0], viewport[2]);
      poly[4 * i + 1] = VIEWPORT_TRANSFORM_Y (poly[4 * i + 1],
                                              viewport[1], viewport[3]);
    }

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y
}

static gboolean
try_checking_point_hits_entry_after_clipping (CoglFramebuffer *framebuffer,
                                              CoglJournalEntry *entry,
                                              float *vertices,
                                              float x,
                                              float y,
                                              gboolean *hit)
{
  gboolean can_software_clip = TRUE;
  gboolean needs_software_clip = FALSE;
  CoglClipStack *clip_entry;

  *hit = TRUE;

  /* Verify that all of the clip stack entries are simple rectangle
   * clips */
  for (clip_entry = entry->clip_stack;
       clip_entry;
       clip_entry = clip_entry->parent)
    {
      if (x < clip_entry->bounds_x0 ||
          x >= clip_entry->bounds_x1 ||
          y < clip_entry->bounds_y0 ||
          y >= clip_entry->bounds_y1)
        {
          *hit = FALSE;
          return TRUE;
        }

      if (clip_entry->type == COGL_CLIP_STACK_WINDOW_RECT)
        {
          /* XXX: technically we could still run the software clip in
           * this case because for our purposes we know this clip
           * can be ignored now, but [can_]sofware_clip_entry() doesn't
           * know this and will bail out. */
          can_software_clip = FALSE;
        }
      else if (clip_entry->type == COGL_CLIP_STACK_RECT)
        {
          CoglClipStackRect *rect_entry = (CoglClipStackRect *)entry;

          if (rect_entry->can_be_scissor == FALSE)
            needs_software_clip = TRUE;
          /* If can_be_scissor is TRUE then we know it's screen
           * aligned and the hit test we did above has determined
           * that we are inside this clip. */
        }
      else
        return FALSE;
    }

  if (needs_software_clip)
    {
      ClipBounds clip_bounds;
      float poly[16];

      if (!can_software_clip)
        return FALSE;

      if (!can_software_clip_entry (entry, NULL,
                                    entry->clip_stack, &clip_bounds))
        return FALSE;

      software_clip_entry (entry, vertices, &clip_bounds);
      entry_to_screen_polygon (framebuffer, entry, vertices, poly);

      *hit = _cogl_util_point_in_screen_poly (x, y, poly, sizeof (float) * 4, 4);
      return TRUE;
    }

  return TRUE;
}

gboolean
_cogl_journal_try_read_pixel (CoglJournal *journal,
                              int x,
                              int y,
                              CoglPixelFormat format,
                              guint8 *pixel,
                              gboolean *found_intersection)
{
  int i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* XXX: this number has been plucked out of thin air, but the idea
   * is that if so many pixels are being read from the same un-changed
   * journal than we expect that it will be more efficient to fail
   * here so we end up flushing and rendering the journal so that
   * further reads can directly read from the framebuffer. There will
   * be a bit more lag to flush the render but if there are going to
   * continue being lots of arbitrary single pixel reads they will end
   * up faster in the end. */
  if (journal->fast_read_pixel_count > 50)
    return FALSE;

  if (format != COGL_PIXEL_FORMAT_RGBA_8888_PRE &&
      format != COGL_PIXEL_FORMAT_RGBA_8888)
    return FALSE;

  *found_intersection = FALSE;

  /* NB: The most recently added journal entry is the last entry, and
   * assuming this is a simple scene only comprised of opaque coloured
   * rectangles with no special pipelines involved (e.g. enabling
   * depth testing) then we can assume painter's algorithm for the
   * entries and so our fast read-pixel just needs to walk backwards
   * through the journal entries trying to intersect each entry with
   * the given point of interest. */
  for (i = journal->entries->len - 1; i >= 0; i--)
    {
      CoglJournalEntry *entry =
        &g_array_index (journal->entries, CoglJournalEntry, i);
      guint8 *color = (guint8 *)&g_array_index (journal->vertices, float,
                                                entry->array_offset);
      float *vertices = (float *)color + 1;
      float poly[16];
      CoglFramebuffer *framebuffer = journal->framebuffer;

      entry_to_screen_polygon (framebuffer, entry, vertices, poly);

      if (!_cogl_util_point_in_screen_poly (x, y, poly, sizeof (float) * 4, 4))
        continue;

      if (entry->clip_stack)
        {
          gboolean hit;

          if (!try_checking_point_hits_entry_after_clipping (framebuffer,
                                                             entry,
                                                             vertices,
                                                             x, y, &hit))
            return FALSE; /* hit couldn't be determined */

          if (!hit)
            continue;
        }

      *found_intersection = TRUE;

      /* If we find that the rectangle the point of interest
       * intersects has any state more complex than a constant opaque
       * color then we bail out. */
      if (!_cogl_pipeline_equal (ctx->opaque_color_pipeline, entry->pipeline,
                                 (COGL_PIPELINE_STATE_ALL &
                                  ~COGL_PIPELINE_STATE_COLOR),
                                 COGL_PIPELINE_LAYER_STATE_ALL,
                                 0))
        return FALSE;


      /* we currently only care about cases where the premultiplied or
       * unpremultipled colors are equivalent... */
      if (color[3] != 0xff)
        return FALSE;

      pixel[0] = color[0];
      pixel[1] = color[1];
      pixel[2] = color[2];
      pixel[3] = color[3];

      goto success;
    }

success:
  journal->fast_read_pixel_count++;
  return TRUE;
}
