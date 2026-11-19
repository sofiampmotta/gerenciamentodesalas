#include "interface.h"

void estilo_aplicar(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        "window { background: #f1f5f9; color: #0f172a; font-family: 'Segoe UI', 'Inter', Arial, sans-serif; font-size: 16px; }"
        ".menu-lateral { background: #1e3148; padding: 20px 0 16px 0; }"
        ".faixa-vermelha { min-height: 0; margin: 0; background: transparent; }"
        ".logo { color: #ffffff; font-size: 28px; font-weight: 800; padding: 0; }"
        ".sublogo { color: #cbd5e1; font-size: 13.5px; font-weight: 600; padding: 0; }"
        "button.menu { background: transparent; background-image: none; color: #e2e8f0; border: none; border-radius: 12px; box-shadow: none; padding: 12px 16px; margin: 3px 14px; font-weight: 600; font-size: 16px; }"
        "button.menu:hover { background: rgba(255, 255, 255, 0.10); }"
        "button.menu:checked { background: #2b82f6; color: #ffffff; font-weight: 700; }"
        ".usuario-nome { color: #ffffff; font-weight: 700; font-size: 15px; }"
        ".usuario-info { color: #cbd5e1; font-size: 13px; font-weight: 500; }"
        ".titulo { font-size: 36px; font-weight: 800; color: #0f172a; }"
        ".subtitulo { color: #475569; font-size: 16px; font-weight: 600; }"
        ".secao { font-size: 21px; font-weight: 700; color: #0f172a; }"
        ".numero-grande, .numero-busca { font-size: 36px; font-weight: 800; color: #0f172a; }"
        "button.primario { background: #2b82f6; background-image: none; color: #ffffff; border: none; border-radius: 12px; font-weight: 700; font-size: 16px; padding: 13px 24px; box-shadow: none; }"
        "button.primario:hover { background: #2563eb; }"
        "button.perigo { background: #dc2626; background-image: none; color: #ffffff; border: none; border-radius: 12px; font-weight: 700; padding: 11px 22px; }"
        "entry, spinbutton, passwordentry { padding: 9px 12px; border-radius: 10px; font-size: 15px; }"
        ".barra-status { margin: 10px 36px 18px 36px; padding: 11px 16px; border-radius: 12px; border: 1px solid #cbd5e1; background: #ffffff; color: #1e293b; font-size: 15px; font-weight: 600; }"
        ".barra-status.success { background: #dcfce7; border-color: #22c55e; color: #14532d; }"
        ".barra-status.error { background: #fee2e2; border-color: #ef4444; color: #7f1d1d; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}
