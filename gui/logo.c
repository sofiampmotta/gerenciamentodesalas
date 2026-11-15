#include "interface.h"
#include <cairo.h>

/* 1 = placa branca atrás do logo (PNG colorido). 0 = sem placa (PNG branco/transparente). */
#define LOGO_FUNDO_BRANCO 1
#define LOGO_LARGURA      150

typedef struct {
    cairo_surface_t *imagem;
    int w, h;
} Logo;

static void liberar(gpointer dados) {
    Logo *l = (Logo *) dados;
    cairo_surface_destroy(l->imagem);
    g_free(l);
}

static void desenhar(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer dados) {
    Logo *l = (Logo *) dados;
    double esc = (double) w / l->w;
    cairo_scale(cr, esc, esc);
    cairo_set_source_surface(cr, l->imagem, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
    cairo_paint(cr);
}

GtkWidget *logo_poli_criar(void) {
    static const char *CAMINHOS[] = { "gui/assets/logo_poli.png", "assets/logo_poli.png", NULL };

    for (int i = 0; CAMINHOS[i] != NULL; i++) {
        if (!g_file_test(CAMINHOS[i], G_FILE_TEST_EXISTS)) continue;

        cairo_surface_t *img = cairo_image_surface_create_from_png(CAMINHOS[i]);
        if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(img);
            continue;
        }
        Logo *l = g_new0(Logo, 1);
        l->imagem = img;
        l->w = cairo_image_surface_get_width(img);
        l->h = cairo_image_surface_get_height(img);
        if (l->w <= 0 || l->h <= 0) { liberar(l); continue; }

        GtkWidget *area = gtk_drawing_area_new();
        int altura = (int) ((double) LOGO_LARGURA * l->h / l->w);
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), LOGO_LARGURA);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), altura);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), desenhar, l, liberar);

        GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_halign(caixa, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(caixa), area);
#if LOGO_FUNDO_BRANCO
        gtk_widget_add_css_class(caixa, "logo-selo");
#else
        gtk_widget_set_margin_start(caixa, 24);
        gtk_widget_set_margin_bottom(caixa, 14);
#endif
        return caixa;
    }
    return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);   /* sem arquivo: nada aparece */
}
