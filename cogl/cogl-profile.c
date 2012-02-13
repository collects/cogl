#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef COGL_ENABLE_PROFILE

#include "cogl-profile.h"
#include "cogl-debug.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

UProfContext *_cogl_uprof_context;

static gboolean
debug_option_getter (void *user_data)
{
  unsigned int shift = GPOINTER_TO_UINT (user_data);
  return COGL_DEBUG_ENABLED (shift);
}

static void
debug_option_setter (gboolean value, void *user_data)
{
  unsigned int shift = GPOINTER_TO_UINT (user_data);

  if (value)
    COGL_DEBUG_SET_FLAG (shift);
  else
    COGL_DEBUG_CLEAR_FLAG (shift);
}

static void
print_exit_report (void)
{
  if (getenv ("COGL_PROFILE_OUTPUT_REPORT"))
    {
      UProfReport *report = uprof_report_new ("Cogl report");
      uprof_report_add_context (report, _cogl_uprof_context);
      uprof_report_print (report);
      uprof_report_unref (report);
    }
  uprof_context_unref (_cogl_uprof_context);
}

void
_cogl_uprof_init (void)
{
  _cogl_uprof_context = uprof_context_new ("Cogl");
#define OPT(MASK_NAME, GROUP, NAME, NAME_FORMATTED, DESCRIPTION) \
  G_STMT_START { \
    int shift = COGL_DEBUG_ ## MASK_NAME; \
    uprof_context_add_boolean_option (_cogl_uprof_context, \
                                      g_dgettext (GETTEXT_PACKAGE, GROUP), \
                                      NAME, \
                                      g_dgettext (GETTEXT_PACKAGE, \
                                                  NAME_FORMATTED), \
                                      g_dgettext (GETTEXT_PACKAGE, \
                                                  DESCRIPTION),    \
                                      debug_option_getter, \
                                      debug_option_setter, \
                                      GUINT_TO_POINTER (shift)); \
  } G_STMT_END;

#include "cogl-debug-options.h"
#undef OPT

  atexit (print_exit_report);
}

void
_cogl_profile_trace_message (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, ap);
  va_end (ap);

  if (_cogl_uprof_context)
    uprof_context_vtrace_message (_cogl_uprof_context, format, ap);
}

#endif
