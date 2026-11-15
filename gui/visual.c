#include "interface.h"

/* ===== TIPOGRAFIA: para testar outra fonte, mude só estas linhas =====
   Sugestões que já vêm no Windows: 'Georgia' (serifa, como no protótipo),
   'Trebuchet MS', 'Verdana', 'Bahnschrift'. */
#define FONTE_TITULOS  "'Segoe UI', Arial, sans-serif"
#define PESO_TITULOS   "700"
#define PESO_BOTOES    "600"

void visual_aplicar(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".titulo, .titulo-form, .secao, .titulo-resultado, .nome-sala-busca,"
        ".numero-grande, .numero-busca, .status-ok, .status-erro"
        "  { font-family: " FONTE_TITULOS "; font-weight: " PESO_TITULOS "; }"
        "button.menu { font-weight: " PESO_BOTOES "; }"
        "button.primario, button.secundario { font-weight: 700; }"
        ".negrito { font-weight: 700; }"
        ".logo-selo { background: #ffffff; border-radius: 8px; padding: 8px 10px;"
        "  margin: 0 24px 14px 24px; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
}
