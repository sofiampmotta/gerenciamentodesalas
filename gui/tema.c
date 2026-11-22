#include "interface.h"

extern const char *CSS_VISUAL_ESCURO;

static GtkCssProvider *g_escuro = NULL;
static gboolean g_ativo = FALSE;

static void aplicar(gboolean escuro) {
  GdkDisplay *display = gdk_display_get_default();
  g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               escuro, NULL);

  if (!g_escuro) {
    g_escuro = gtk_css_provider_new();
    gtk_css_provider_load_from_string(g_escuro, CSS_VISUAL_ESCURO);
  }
  if (escuro && !g_ativo) {
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(g_escuro),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    g_ativo = TRUE;
  } else if (!escuro && g_ativo) {
    gtk_style_context_remove_provider_for_display(display,
                                                  GTK_STYLE_PROVIDER(g_escuro));
    g_ativo = FALSE;
  }
}

static void ao_trocar(GObject *o, GParamSpec *p, gpointer dados) {
  aplicar(gtk_switch_get_active(GTK_SWITCH(o)));
}

GtkWidget *tema_criar_chave(void) {
  aplicar(FALSE);

  GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_bottom(linha, 4);
  gtk_widget_set_margin_start(linha, 14);

  GtkWidget *rotulo = gtk_label_new("Modo noturno");
  gtk_widget_add_css_class(rotulo, "usuario-info");
  gtk_label_set_xalign(GTK_LABEL(rotulo), 0);
  gtk_widget_set_hexpand(rotulo, TRUE);

  GtkWidget *chave = gtk_switch_new();
  gtk_widget_set_valign(chave, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end(chave, 14);
  g_signal_connect(chave, "notify::active", G_CALLBACK(ao_trocar), NULL);

  gtk_box_append(GTK_BOX(linha), rotulo);
  gtk_box_append(GTK_BOX(linha), chave);
  return linha;
}
