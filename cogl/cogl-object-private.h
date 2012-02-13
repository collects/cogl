/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_OBJECT_PRIVATE_H
#define __COGL_OBJECT_PRIVATE_H

#include <glib.h>

#include "cogl-types.h"
#include "cogl-object.h"
#include "cogl-debug.h"

/* For compatability until all components have been converted */
typedef struct _CoglObjectClass CoglHandleClass;
typedef struct _CoglObject      CoglHandleObject;

/* XXX: sadly we didn't fully consider when we copied the cairo API
 * for _set_user_data that the callback doesn't get a pointer to the
 * instance which is desired in most cases. This means you tend to end
 * up creating micro allocations for the private data just so you can
 * pair up the data of interest with the original instance for
 * identification when it is later destroyed.
 *
 * Internally we use a small hack to avoid needing these micro
 * allocations by actually passing the instance as a second argument
 * to the callback */
typedef void (*CoglUserDataDestroyInternalCallback) (void *user_data,
                                                     void *instance);

typedef struct _CoglObjectClass
{
  const char *name;
  void *virt_free;
  void *virt_unref;
} CoglObjectClass;

#define COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES 2

typedef struct
{
  CoglUserDataKey *key;
  void *user_data;
  CoglUserDataDestroyInternalCallback destroy;
} CoglUserDataEntry;

/* All Cogl objects inherit from this base object by adding a member:
 *
 *   CoglObject _parent;
 *
 * at the top of its main structure. This structure is initialized
 * when you call _cogl_#type_name#_object_new (new_object);
 */
struct _CoglObject
{
  CoglObjectClass  *klass;

  CoglUserDataEntry user_data_entry[
    COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES];
  GArray           *user_data_array;
  int               n_user_data_entries;

  unsigned int      ref_count;
};

/* Helper macro to encapsulate the common code for COGL reference
   counted objects */

#ifdef COGL_OBJECT_DEBUG

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)                          \
  COGL_NOTE (HANDLE, "COGL " G_STRINGIFY (type_name) " NEW   %p %i",    \
             (obj), (obj)->ref_count)

#define _COGL_OBJECT_DEBUG_REF(type_name, object)       G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (HANDLE, "COGL %s REF %p %i",                               \
             (__obj)->klass->name,                                      \
             (__obj), (__obj)->ref_count);              } G_STMT_END

#define _COGL_OBJECT_DEBUG_UNREF(type_name, object)     G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (HANDLE, "COGL %s UNREF %p %i",                             \
             (__obj)->klass->name,                                      \
             (__obj), (__obj)->ref_count - 1);          } G_STMT_END

#define COGL_OBJECT_DEBUG_FREE(obj)                                     \
  COGL_NOTE (HANDLE, "COGL %s FREE %p",                                 \
             (obj)->klass->name, (obj))

#else /* !COGL_OBJECT_DEBUG */

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)
#define _COGL_OBJECT_DEBUG_REF(type_name, obj)
#define _COGL_OBJECT_DEBUG_UNREF(type_name, obj)
#define COGL_OBJECT_DEBUG_FREE(obj)

#endif /* COGL_OBJECT_DEBUG */

/* For temporary compatability */
#define _COGL_HANDLE_DEBUG_NEW _COGL_OBJECT_DEBUG_NEW
#define _COGL_HANDLE_DEBUG_REF _COGL_OBJECT_DEBUG_REF
#define _COGL_HANDLE_DEBUG_UNREF _COGL_OBJECT_DEBUG_UNREF
#define COGL_HANDLE_DEBUG_FREE COGL_OBJECT_DEBUG_FREE

#define COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)  \
                                                                        \
CoglObjectClass _cogl_##type_name##_class;                              \
static unsigned long _cogl_object_##type_name##_count;                  \
                                                                        \
static inline void                                                      \
_cogl_object_##type_name##_inc (void)                                   \
{                                                                       \
  _cogl_object_##type_name##_count++;                                   \
}                                                                       \
                                                                        \
static inline void                                                      \
_cogl_object_##type_name##_dec (void)                                   \
{                                                                       \
  _cogl_object_##type_name##_count--;                                   \
}                                                                       \
                                                                        \
static void                                                             \
_cogl_object_##type_name##_indirect_free (CoglObject *obj)              \
{                                                                       \
  _cogl_##type_name##_free ((Cogl##TypeName *) obj);                    \
  _cogl_object_##type_name##_dec ();                                    \
}                                                                       \
                                                                        \
static Cogl##TypeName *                                                 \
_cogl_##type_name##_object_new (Cogl##TypeName *new_obj)                \
{                                                                       \
  CoglObject *obj = (CoglObject *)&new_obj->_parent;                    \
  obj->ref_count = 0;                                                   \
  cogl_object_ref (obj);                                                \
  obj->n_user_data_entries = 0;                                         \
  obj->user_data_array = NULL;                                          \
                                                                        \
  obj->klass = &_cogl_##type_name##_class;                              \
  if (!obj->klass->virt_free)                                           \
    {                                                                   \
      _cogl_object_##type_name##_count = 0;                             \
                                                                        \
      if (_cogl_debug_instances == NULL)                                \
        _cogl_debug_instances =                                         \
          g_hash_table_new (g_str_hash, g_str_equal);                   \
                                                                        \
      obj->klass->virt_free =                                           \
        _cogl_object_##type_name##_indirect_free;                       \
      obj->klass->virt_unref =                                          \
        _cogl_object_default_unref;                                     \
      obj->klass->name = "Cogl"#TypeName,                               \
                                                                        \
      g_hash_table_insert (_cogl_debug_instances,                       \
                           (void *) obj->klass->name,                   \
                           &_cogl_object_##type_name##_count);          \
                                                                        \
      { code; }                                                         \
    }                                                                   \
                                                                        \
  _cogl_object_##type_name##_inc ();                                    \
  _COGL_OBJECT_DEBUG_NEW (TypeName, obj);                               \
  return new_obj;                                                       \
}                                                                       \
                                                                        \
Cogl##TypeName *                                                        \
_cogl_##type_name##_pointer_from_handle (CoglHandle handle)             \
{                                                                       \
  return handle;                                                        \
}

#define COGL_OBJECT_DEFINE_WITH_CODE(TypeName, type_name, code)         \
                                                                        \
COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                        \
gboolean                                                                \
cogl_is_##type_name (void *object)                                      \
{                                                                       \
  CoglObject *obj = object;                                             \
                                                                        \
  if (object == NULL)                                                   \
    return FALSE;                                                       \
                                                                        \
  return obj->klass == &_cogl_##type_name##_class;                      \
}

#define COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                        \
COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                        \
gboolean                                                                \
_cogl_is_##type_name (void *object)                                     \
{                                                                       \
  CoglObject *obj = object;                                             \
                                                                        \
  if (object == NULL)                                                   \
    return FALSE;                                                       \
                                                                        \
  return obj->klass == &_cogl_##type_name##_class;                      \
}

#define COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING(type_name)   \
                                                                \
void * G_GNUC_DEPRECATED                                        \
cogl_##type_name##_ref (void *object)                           \
{                                                               \
  if (!cogl_is_##type_name (object))                            \
    return NULL;                                                \
                                                                \
  _COGL_OBJECT_DEBUG_REF (TypeName, object);                    \
                                                                \
  cogl_handle_ref (object);                                     \
                                                                \
  return object;                                                \
}                                                               \
                                                                \
void G_GNUC_DEPRECATED                                          \
cogl_##type_name##_unref (void *object)                         \
{                                                               \
  if (!cogl_is_##type_name (object))                            \
    {                                                           \
      g_warning (G_STRINGIFY (cogl_##type_name##_unref)         \
                 ": Ignoring unref of Cogl handle "             \
                 "due to type mismatch");                       \
      return;                                                   \
    }                                                           \
                                                                \
  _COGL_OBJECT_DEBUG_UNREF (TypeName, object);                  \
                                                                \
  cogl_handle_unref (object);                                   \
}

#define COGL_OBJECT_DEFINE(TypeName, type_name)                 \
  COGL_OBJECT_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

#define COGL_OBJECT_INTERNAL_DEFINE(TypeName, type_name)         \
  COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

/* For temporary compatability */
#define COGL_HANDLE_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                         \
COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE (TypeName, type_name, code)        \
                                                                         \
static Cogl##TypeName *                                                  \
_cogl_##type_name##_handle_new (CoglHandle handle)                       \
{                                                                        \
  return _cogl_##type_name##_object_new (handle);                        \
}

#define COGL_HANDLE_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                         \
COGL_OBJECT_DEFINE_WITH_CODE (TypeName, type_name, code)                 \
                                                                         \
static Cogl##TypeName *                                                  \
_cogl_##type_name##_handle_new (CoglHandle handle)                       \
{                                                                        \
  return _cogl_##type_name##_object_new (handle);                        \
}

#define COGL_HANDLE_INTERNAL_DEFINE(TypeName, type_name)        \
  COGL_HANDLE_INTERNAL_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

#define COGL_HANDLE_DEFINE(TypeName, type_name)                 \
  COGL_HANDLE_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

void
_cogl_object_set_user_data (CoglObject *object,
                            CoglUserDataKey *key,
                            void *user_data,
                            CoglUserDataDestroyInternalCallback destroy);

void
_cogl_object_default_unref (void *obj);

#endif /* __COGL_OBJECT_PRIVATE_H */

