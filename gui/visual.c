#include "interface.h"

/* Só a fonte e a placa do logo. Para testar outra fonte, mude só esta linha. */
#define FONTE_TEXTO "Inter, 'Inter UI', 'Inter 18pt', 'Segoe UI', Arial, sans-serif"

void visual_aplicar(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        "* { font-family: " FONTE_TEXTO "; }"
        ".logo, .logo-cartao { font-family: Georgia, serif; }"
        ".logo-selo { background: #ffffff; border-radius: 8px; padding: 8px 10px;"
        "  margin: 0 24px 14px 24px; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
}
