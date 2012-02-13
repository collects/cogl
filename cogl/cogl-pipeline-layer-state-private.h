/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PIPELINE_LAYER_STATE_PRIVATE_H
#define __COGL_PIPELINE_LAYER_STATE_PRIVATE_H

#include "cogl-pipeline-layer-state.h"
#include "cogl-pipeline-private.h"

CoglPipelineLayer *
_cogl_pipeline_set_layer_unit (CoglPipeline *required_owner,
                               CoglPipelineLayer *layer,
                               int unit_index);

CoglPipelineFilter
_cogl_pipeline_get_layer_min_filter (CoglPipeline *pipeline,
                                     int layer_index);

CoglPipelineFilter
_cogl_pipeline_get_layer_mag_filter (CoglPipeline *pipeline,
                                     int layer_index);

gboolean
_cogl_pipeline_layer_texture_target_equal (CoglPipelineLayer *authority0,
                                           CoglPipelineLayer *authority1,
                                           CoglPipelineEvalFlags flags);

gboolean
_cogl_pipeline_layer_texture_data_equal (CoglPipelineLayer *authority0,
                                         CoglPipelineLayer *authority1,
                                         CoglPipelineEvalFlags flags);

gboolean
_cogl_pipeline_layer_combine_state_equal (CoglPipelineLayer *authority0,
                                          CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_combine_constant_equal (CoglPipelineLayer *authority0,
                                             CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_filters_equal (CoglPipelineLayer *authority0,
                                    CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_wrap_modes_equal (CoglPipelineLayer *authority0,
                                       CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_user_matrix_equal (CoglPipelineLayer *authority0,
                                        CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_point_sprite_coords_equal (CoglPipelineLayer *authority0,
                                                CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_vertex_snippets_equal (CoglPipelineLayer *authority0,
                                            CoglPipelineLayer *authority1);

gboolean
_cogl_pipeline_layer_fragment_snippets_equal (CoglPipelineLayer *authority0,
                                              CoglPipelineLayer *authority1);

void
_cogl_pipeline_layer_hash_unit_state (CoglPipelineLayer *authority,
                                      CoglPipelineLayer **authorities,
                                      CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_texture_target_state (CoglPipelineLayer *authority,
                                                CoglPipelineLayer **authorities,
                                                CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_texture_data_state (CoglPipelineLayer *authority,
                                              CoglPipelineLayer **authorities,
                                              CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_filters_state (CoglPipelineLayer *authority,
                                         CoglPipelineLayer **authorities,
                                         CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_wrap_modes_state (CoglPipelineLayer *authority,
                                            CoglPipelineLayer **authorities,
                                            CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_combine_state (CoglPipelineLayer *authority,
                                         CoglPipelineLayer **authorities,
                                         CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_combine_constant_state (CoglPipelineLayer *authority,
                                                  CoglPipelineLayer **authorities,
                                                  CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_user_matrix_state (CoglPipelineLayer *authority,
                                             CoglPipelineLayer **authorities,
                                             CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_point_sprite_state (CoglPipelineLayer *authority,
                                              CoglPipelineLayer **authorities,
                                              CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_vertex_snippets_state (CoglPipelineLayer *authority,
                                                 CoglPipelineLayer **authorities,
                                                 CoglPipelineHashState *state);

void
_cogl_pipeline_layer_hash_fragment_snippets_state (CoglPipelineLayer *authority,
                                                   CoglPipelineLayer **authorities,
                                                   CoglPipelineHashState *state);

#endif /* __COGL_PIPELINE_LAYER_STATE_PRIVATE_H */
