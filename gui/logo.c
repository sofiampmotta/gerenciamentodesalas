#include "interface.h"
#include <cairo.h>

#define LOGO_LARGURA 110

typedef struct { cairo_surface_t *img; int w, h; } Logo;

static void liberar(gpointer d) {
    Logo *l = d;
    cairo_surface_destroy(l->img);
    g_free(l);
}

static void desenhar(GtkDrawingArea *a, cairo_t *cr, int w, int h, gpointer d) {
    Logo *l = d;
    double e = (double) w / l->w;
    cairo_scale(cr, e, e);
    cairo_set_source_surface(cr, l->img, 0, 0);
    cairo_paint(cr);
}

static gchar *procurar(void) {
    const char *pastas[] = { "gui/assets", "assets", NULL };
    for (int i = 0; pastas[i]; i++) {
        gchar *p = g_build_filename(pastas[i], "logo_upe.png", NULL);
        if (g_file_test(p, G_FILE_TEST_IS_REGULAR)) return p;
        g_free(p);
    }
    return NULL;
}

GtkWidget *logo_poli_criar(void) {
    gchar *caminho = procurar();
    if (!caminho) {
        g_print("[logo] gui/assets/logo_upe.png nao encontrado\n");
        return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    }
    cairo_surface_t *img = cairo_image_surface_create_from_png(caminho);
    g_free(caminho);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        g_print("[logo] nao consegui abrir o PNG\n");
        cairo_surface_destroy(img);
        return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    }

    Logo *l = g_new0(Logo, 1);
    l->img = img;
    l->w = cairo_image_surface_get_width(img);
    l->h = cairo_image_surface_get_height(img);
    if (l->w <= 0 || l->h <= 0) {
        liberar(l);
        return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    }
    int altura = LOGO_LARGURA * l->h / l->w;

    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), LOGO_LARGURA);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), altura);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), desenhar, l, liberar);

    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(caixa, GTK_ALIGN_START);
    gtk_widget_add_css_class(caixa, "logo-selo");
    gtk_box_append(GTK_BOX(caixa), area);
    g_print("[logo] carregado (%dx%d)\n", l->w, l->h);
    return caixa;
}