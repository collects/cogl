/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "cogl-bitmask.h"
#include "cogl-util.h"
#include "cogl-flags.h"

/* This code assumes that we can cast an unsigned long to a pointer
   and back without losing any data */
G_STATIC_ASSERT (sizeof (unsigned long) <= sizeof (void *));

#define ARRAY_INDEX(bit_num) \
  ((bit_num) / (sizeof (unsigned long) * 8))
#define BIT_INDEX(bit_num) \
  ((bit_num) & (sizeof (unsigned long) * 8 - 1))
#define BIT_MASK(bit_num) \
  (1UL << BIT_INDEX (bit_num))

gboolean
_cogl_bitmask_get_from_array (const CoglBitmask *bitmask,
                              unsigned int bit_num)
{
  GArray *array = (GArray *) *bitmask;

  /* If the index is off the end of the array then assume the bit is
     not set */
  if (bit_num >= sizeof (unsigned long) * 8 * array->len)
    return FALSE;
  else
    return !!(g_array_index (array, unsigned long, ARRAY_INDEX (bit_num)) &
              BIT_MASK (bit_num));
}

static void
_cogl_bitmask_convert_to_array (CoglBitmask *bitmask)
{
  GArray *array;
  /* Fetch the old values */
  unsigned long old_values = _cogl_bitmask_to_bits (bitmask);

  array = g_array_new (FALSE, /* not zero-terminated */
                       TRUE, /* do clear new entries */
                       sizeof (unsigned long));
  /* Copy the old values back in */
  g_array_append_val (array, old_values);

  *bitmask = (struct _CoglBitmaskImaginaryType *) array;
}

void
_cogl_bitmask_set_in_array (CoglBitmask *bitmask,
                            unsigned int bit_num,
                            gboolean value)
{
  GArray *array;
  unsigned int array_index;
  unsigned long new_value_mask;

  /* If the bitmask is not already an array then we need to allocate one */
  if (!_cogl_bitmask_has_array (bitmask))
    _cogl_bitmask_convert_to_array (bitmask);

  array = (GArray *) *bitmask;

  array_index = ARRAY_INDEX (bit_num);
  /* Grow the array if necessary. This will clear the new data */
  if (array_index >= array->len)
    g_array_set_size (array, array_index + 1);

  new_value_mask = BIT_MASK (bit_num);

  if (value)
    g_array_index (array, unsigned long, array_index) |= new_value_mask;
  else
    g_array_index (array, unsigned long, array_index) &= ~new_value_mask;
}

void
_cogl_bitmask_set_bits (CoglBitmask *dst,
                        const CoglBitmask *src)
{
  if (_cogl_bitmask_has_array (src))
    {
      GArray *src_array, *dst_array;
      int i;

      if (!_cogl_bitmask_has_array (dst))
        _cogl_bitmask_convert_to_array (dst);

      dst_array = (GArray *) *dst;
      src_array = (GArray *) *src;

      if (dst_array->len < src_array->len)
        g_array_set_size (dst_array, src_array->len);

      for (i = 0; i < src_array->len; i++)
        g_array_index (dst_array, unsigned long, i) |=
          g_array_index (src_array, unsigned long, i);
    }
  else if (_cogl_bitmask_has_array (dst))
    {
      GArray *dst_array;

      dst_array = (GArray *) *dst;

      g_array_index (dst_array, unsigned long, 0) |=
        _cogl_bitmask_to_bits (src);
    }
  else
    *dst = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (dst) |
                                    _cogl_bitmask_to_bits (src));
}

void
_cogl_bitmask_set_range_in_array (CoglBitmask *bitmask,
                                  unsigned int n_bits,
                                  gboolean value)
{
  GArray *array;
  unsigned int array_index, bit_index;

  if (n_bits == 0)
    return;

  /* If the bitmask is not already an array then we need to allocate one */
  if (!_cogl_bitmask_has_array (bitmask))
    _cogl_bitmask_convert_to_array (bitmask);

  array = (GArray *) *bitmask;

  /* Get the array index of the top most value that will be touched */
  array_index = ARRAY_INDEX (n_bits - 1);
  /* Get the bit index of the top most value */
  bit_index = BIT_INDEX (n_bits - 1);
  /* Grow the array if necessary. This will clear the new data */
  if (array_index >= array->len)
    g_array_set_size (array, array_index + 1);

  if (value)
    {
      /* Set the bits that are touching this index */
      g_array_index (array, unsigned long, array_index) |=
        ~0UL >> (sizeof (unsigned long) * 8 - 1 - bit_index);

      /* Set all of the bits in any lesser indices */
      memset (array->data, 0xff, sizeof (unsigned long) * array_index);
    }
  else
    {
      /* Clear the bits that are touching this index */
      g_array_index (array, unsigned long, array_index) &= ~1UL << bit_index;

      /* Clear all of the bits in any lesser indices */
      memset (array->data, 0x00, sizeof (unsigned long) * array_index);
    }
}

void
_cogl_bitmask_xor_bits (CoglBitmask *dst,
                        const CoglBitmask *src)
{
  if (_cogl_bitmask_has_array (src))
    {
      GArray *src_array, *dst_array;
      int i;

      if (!_cogl_bitmask_has_array (dst))
        _cogl_bitmask_convert_to_array (dst);

      dst_array = (GArray *) *dst;
      src_array = (GArray *) *src;

      if (dst_array->len < src_array->len)
        g_array_set_size (dst_array, src_array->len);

      for (i = 0; i < src_array->len; i++)
        g_array_index (dst_array, unsigned long, i) ^=
          g_array_index (src_array, unsigned long, i);
    }
  else if (_cogl_bitmask_has_array (dst))
    {
      GArray *dst_array;

      dst_array = (GArray *) *dst;

      g_array_index (dst_array, unsigned long, 0) ^=
        _cogl_bitmask_to_bits (src);
    }
  else
    *dst = _cogl_bitmask_from_bits (_cogl_bitmask_to_bits (dst) ^
                                    _cogl_bitmask_to_bits (src));
}

void
_cogl_bitmask_clear_all_in_array (CoglBitmask *bitmask)
{
  GArray *array = (GArray *) *bitmask;

  memset (array->data, 0, sizeof (unsigned long) * array->len);
}

void
_cogl_bitmask_foreach (const CoglBitmask *bitmask,
                       CoglBitmaskForeachFunc func,
                       void *user_data)
{
  if (_cogl_bitmask_has_array (bitmask))
    {
      GArray *array = (GArray *) *bitmask;
      const unsigned long *values = &g_array_index (array, unsigned long, 0);
      int bit_num;

      COGL_FLAGS_FOREACH_START (values, array->len, bit_num)
        {
          if (!func (bit_num, user_data))
            return;
        }
      COGL_FLAGS_FOREACH_END;
    }
  else
    {
      unsigned long mask = _cogl_bitmask_to_bits (bitmask);
      int bit_num;

      COGL_FLAGS_FOREACH_START (&mask, 1, bit_num)
        {
          if (!func (bit_num, user_data))
            return;
        }
      COGL_FLAGS_FOREACH_END;
    }
}

void
_cogl_bitmask_set_flags_array (const CoglBitmask *bitmask,
                               unsigned long *flags)
{
  const GArray *array = (const GArray *) *bitmask;
  int i;

  for (i = 0; i < array->len; i++)
    flags[i] |= g_array_index (array, unsigned long, i);
}

int
_cogl_bitmask_popcount_in_array (const CoglBitmask *bitmask)
{
  const GArray *array = (const GArray *) *bitmask;
  int pop = 0;
  int i;

  for (i = 0; i < array->len; i++)
    pop += _cogl_util_popcountl (g_array_index (array, unsigned long, i));

  return pop;
}

int
_cogl_bitmask_popcount_upto_in_array (const CoglBitmask *bitmask,
                                      int upto)
{
  const GArray *array = (const GArray *) *bitmask;

  if (upto >= array->len * sizeof (unsigned long) * 8)
    return _cogl_bitmask_popcount_in_array (bitmask);
  else
    {
      unsigned long top_mask;
      int array_index = ARRAY_INDEX (upto);
      int bit_index = BIT_INDEX (upto);
      int pop = 0;
      int i;

      for (i = 0; i < array_index; i++)
        pop += _cogl_util_popcountl (g_array_index (array, unsigned long, i));

      top_mask = g_array_index (array, unsigned long, array_index);

      return pop + _cogl_util_popcountl (top_mask & ((1UL << bit_index) - 1));
    }
}
