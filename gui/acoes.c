#include "interface.h"
#include <stdarg.h>

/* Registra interações da interface para depuração. */
void acao_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);
  g_print("[GUI -> backend] %s\n", msg);
  g_free(msg);
}

const char *acao_hora(int min) {
  static char buf[4][8];
  static int i = 0;
  i = (i + 1) % 4;
  g_snprintf(buf[i], sizeof buf[i], "%02d:%02d", min / 60, min % 60);
  return buf[i];
}
