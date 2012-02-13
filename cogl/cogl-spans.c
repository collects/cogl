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

#include "math.h"

#include "cogl.h"
#include "cogl-util.h"
#include "cogl-internal.h"
#include "cogl-spans.h"

void
_cogl_span_iter_update (CoglSpanIter *iter)
{
  /* Pick current span */
  iter->span = &iter->spans[iter->index];

  /* Offset next position by span size */
  iter->next_pos = iter->pos + iter->span->size - iter->span->waste;

  /* Check if span intersects the area to cover */
  if (iter->next_pos <= iter->cover_start ||
      iter->pos >= iter->cover_end)
    {
      /* Intersection undefined */
      iter->intersects = FALSE;
      return;
    }

  iter->intersects = TRUE;

  /* Clip start position to coverage area */
  if (iter->pos < iter->cover_start)
    iter->intersect_start = iter->cover_start;
  else
    iter->intersect_start = iter->pos;

  /* Clip end position to coverage area */
  if (iter->next_pos > iter->cover_end)
    iter->intersect_end = iter->cover_end;
  else
    iter->intersect_end = iter->next_pos;
}

void
_cogl_span_iter_begin (CoglSpanIter *iter,
                       const CoglSpan *spans,
                       int n_spans,
                       float normalize_factor,
                       float cover_start,
                       float cover_end,
                       CoglPipelineWrapMode wrap_mode)
{
  /* XXX: If CLAMP_TO_EDGE needs to be emulated then it needs to be
   * done at a higher level than here... */
  _COGL_RETURN_IF_FAIL (wrap_mode == COGL_PIPELINE_WRAP_MODE_REPEAT ||
                        wrap_mode == COGL_PIPELINE_WRAP_MODE_MIRRORED_REPEAT);

  iter->span = NULL;

  iter->spans = spans;
  iter->n_spans = n_spans;

  /* We always iterate in a positive direction from the origin. If
   * iter->flipped == TRUE that means whoever is using this API should
   * interpreted the current span as extending in the opposite direction. I.e.
   * it extends to the left if iterating the X axis, or up if the Y axis. */
  if (cover_start > cover_end)
    {
      float tmp = cover_start;
      cover_start = cover_end;
      cover_end = tmp;
      iter->flipped = TRUE;
    }
  else
    iter->flipped = FALSE;

  /* The texture spans cover the normalized texture coordinate space ranging
   * from [0,1] but to help support repeating of sliced textures we allow
   * iteration of any range so we need to relate the start of the range to the
   * nearest point equivalent to 0.
   */
  if (normalize_factor != 1.0)
    {
      float cover_start_normalized = cover_start / normalize_factor;
      iter->origin = floorf (cover_start_normalized) * normalize_factor;
    }
  else
    iter->origin = floorf (cover_start);

  iter->wrap_mode = wrap_mode;

  if (wrap_mode == COGL_PIPELINE_WRAP_MODE_REPEAT)
    iter->index = 0;
  else if (wrap_mode == COGL_PIPELINE_WRAP_MODE_MIRRORED_REPEAT)
    {
      if ((int)iter->origin % 2)
        {
          iter->index = iter->n_spans - 1;
          iter->mirror_direction = -1;
          iter->flipped = !iter->flipped;
        }
      else
        {
          iter->index = 0;
          iter->mirror_direction = 1;
        }
    }
  else
    g_warn_if_reached ();

  iter->cover_start = cover_start;
  iter->cover_end = cover_end;
  iter->pos = iter->origin;

  /* Update intersection */
  _cogl_span_iter_update (iter);

  while (iter->next_pos <= iter->cover_start)
    _cogl_span_iter_next (iter);
}

void
_cogl_span_iter_next (CoglSpanIter *iter)
{
  /* Move current position */
  iter->pos = iter->next_pos;

  if (iter->wrap_mode == COGL_PIPELINE_WRAP_MODE_REPEAT)
    iter->index = (iter->index + 1) % iter->n_spans;
  else if (iter->wrap_mode == COGL_PIPELINE_WRAP_MODE_MIRRORED_REPEAT)
    {
      iter->index += iter->mirror_direction;
      if (iter->index == iter->n_spans || iter->index == -1)
        {
          iter->mirror_direction = -iter->mirror_direction;
          iter->index += iter->mirror_direction;
          iter->flipped = !iter->flipped;
        }
    }
  else
    g_warn_if_reached ();

  /* Update intersection */
  _cogl_span_iter_update (iter);
}

gboolean
_cogl_span_iter_end (CoglSpanIter *iter)
{
  /* End reached when whole area covered */
  return iter->pos >= iter->cover_end;
}


