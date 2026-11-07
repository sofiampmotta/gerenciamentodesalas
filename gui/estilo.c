#include "interface.h"

void estilo_aplicar(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        "window { background: #ffffff; color: #12274f; font-family: 'Segoe UI', sans-serif; font-size: 14px; }"

        ".menu-lateral { background: #12274f; padding: 24px 0 16px 0; }"
        ".faixa-vermelha { background: #c8102e; min-width: 28px; min-height: 3px; margin: 0 24px 10px 24px; }"
        ".logo { color: #ffffff; font-family: Georgia, serif; font-size: 26px; padding: 0 24px; }"
        ".sublogo { color: #aab6cf; font-size: 12px; padding: 0 24px 24px 24px; }"
        "button.menu { background: transparent; background-image: none; color: #cfd8e8; border: none; border-radius: 0; box-shadow: none; border-left: 3px solid transparent; padding: 11px 21px; }"
        "button.menu:hover { background: #1a3363; }"
        "button.menu:checked { background: #1e3a70; color: #ffffff; font-weight: bold; border-left: 3px solid #c8102e; }"
        ".usuario-nome { color: #ffffff; font-weight: bold; padding: 0 24px; }"
        ".usuario-info { color: #aab6cf; font-size: 12px; padding: 0 24px; }"

        ".titulo { font-family: Georgia, serif; font-size: 28px; color: #12274f; }"
        ".subtitulo { color: #5b6b8a; }"

        "button.primario { background: #1f4e8c; background-image: none; color: #ffffff; border: none; border-radius: 4px; font-weight: bold; padding: 10px 20px; box-shadow: none; }"
        "button.primario:hover { background: #18407a; }"
        "button.perigo { background: #c8102e; background-image: none; color: #ffffff; border: none; border-radius: 4px; font-weight: bold; padding: 10px 20px; }"
        "entry { padding: 8px; border-radius: 4px; }"

        ".barra-status { padding: 8px 24px; color: #5b6b8a; border-top: 1px solid #e1e6ef; }"
        ".barra-status.success { background: #1e7e4a; color: #ffffff; }"
        ".barra-status.error { background: #c8102e; color: #ffffff; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}
