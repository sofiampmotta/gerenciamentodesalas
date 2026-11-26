#include "interface.h"
#include <stdlib.h>

static void remover_fundo_externo_suave(GdkPixbuf *rgba) {
  int w = gdk_pixbuf_get_width(rgba);
  int h = gdk_pixbuf_get_height(rgba);
  int stride = gdk_pixbuf_get_rowstride(rgba);
  int n_ch = gdk_pixbuf_get_n_channels(rgba);
  guchar *px = gdk_pixbuf_get_pixels(rgba);
  if (n_ch < 4 || w <= 0 || h <= 0)
    return;

  int total = w * h;
  guchar *visitado = g_new0(guchar, total);
  int *fila = g_new(int, total);
  int ini = 0, fim = 0;

  /* Semeia apenas pixels claros/brancos nas bordas externas */
  for (int x = 0; x < w; x++) {
    int top_idx = x;
    guchar *pt = px + top_idx * n_ch;
    if (pt[0] >= 235 && pt[1] >= 235 && pt[2] >= 235) {
      visitado[top_idx] = 1;
      fila[fim++] = top_idx;
    }
    int bot_idx = (h - 1) * w + x;
    guchar *pb = px + (h - 1) * stride + x * n_ch;
    if (pb[0] >= 235 && pb[1] >= 235 && pb[2] >= 235 && !visitado[bot_idx]) {
      visitado[bot_idx] = 1;
      fila[fim++] = bot_idx;
    }
  }
  for (int y = 0; y < h; y++) {
    int left_idx = y * w;
    guchar *pl = px + y * stride;
    if (pl[0] >= 235 && pl[1] >= 235 && pl[2] >= 235 && !visitado[left_idx]) {
      visitado[left_idx] = 1;
      fila[fim++] = left_idx;
    }
    int right_idx = y * w + (w - 1);
    guchar *pr = px + y * stride + (w - 1) * n_ch;
    if (pr[0] >= 235 && pr[1] >= 235 && pr[2] >= 235 && !visitado[right_idx]) {
      visitado[right_idx] = 1;
      fila[fim++] = right_idx;
    }
  }

  const int dx[4] = {1, -1, 0, 0};
  const int dy[4] = {0, 0, 1, -1};

  while (ini < fim) {
    int idx = fila[ini++];
    int cx = idx % w;
    int cy = idx / w;
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx[k];
      int ny = cy + dy[k];
      if (nx < 0 || nx >= w || ny < 0 || ny >= h)
        continue;
      int nidx = ny * w + nx;
      if (visitado[nidx])
        continue;
      guchar *np = px + ny * stride + nx * n_ch;
      if (np[0] >= 235 && np[1] >= 235 && np[2] >= 235) {
        visitado[nidx] = 1;
        fila[fim++] = nidx;
      }
    }
  }

  /* Torna transparente apenas o fundo externo conectado às bordas */
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = y * w + x;
      if (visitado[idx]) {
        guchar *p = px + y * stride + x * n_ch;
        p[3] = 0;
      }
    }
  }

  /* Suaviza borda de contato entre transparente e opaco para evitar serrilhado
   */
  guchar *copia_alpha = g_new(guchar, total);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      copia_alpha[y * w + x] = (px + y * stride + x * n_ch)[3];
    }
  }
  for (int y = 1; y < h - 1; y++) {
    for (int x = 1; x < w - 1; x++) {
      int idx = y * w + x;
      if (copia_alpha[idx] == 0) {
        int viz_opaco = 0;
        for (int oy = -1; oy <= 1; oy++) {
          for (int ox = -1; ox <= 1; ox++) {
            if (ox == 0 && oy == 0)
              continue;
            if (copia_alpha[(y + oy) * w + (x + ox)] > 128) {
              viz_opaco++;
            }
          }
        }
        if (viz_opaco > 0) {
          guchar *p = px + y * stride + x * n_ch;
          int alfa = (viz_opaco * 140) / 8;
          if (alfa > 140)
            alfa = 140;
          p[3] = (guchar)alfa;
        }
      }
    }
  }

  g_free(copia_alpha);
  g_free(fila);
  g_free(visitado);
}

static GdkPixbuf *carregar_logo_pixbuf(const char *caminho_primario,
                                       const char *caminho_fallback,
                                       int altura_alvo, int max_largura) {
  const char *caminho =
      (caminho_primario && g_file_test(caminho_primario, G_FILE_TEST_EXISTS))
          ? caminho_primario
          : caminho_fallback;
  if (!caminho || !g_file_test(caminho, G_FILE_TEST_EXISTS))
    return NULL;

  int w = 0, h = 0;
  if (!gdk_pixbuf_get_file_info(caminho, &w, &h) || w <= 0 || h <= 0)
    return NULL;

  int largura_alvo = (w * altura_alvo + h / 2) / h;
  if (largura_alvo > max_largura) {
    largura_alvo = max_largura;
    altura_alvo = (h * largura_alvo + w / 2) / w;
  }
  if (largura_alvo < 1)
    largura_alvo = 1;
  if (altura_alvo < 1)
    altura_alvo = 1;

  GError *err = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(caminho, largura_alvo,
                                                    altura_alvo, TRUE, &err);
  if (!pb) {
    if (err)
      g_error_free(err);
    return NULL;
  }

  GdkPixbuf *rgba = gdk_pixbuf_add_alpha(pb, FALSE, 0, 0, 0);
  g_object_unref(pb);
  if (!rgba)
    return NULL;

  remover_fundo_externo_suave(rgba);
  return rgba;
}

static GtkWidget *criar_picture_logo(GdkPixbuf *pb) {
  if (!pb)
    return NULL;
  int pw = gdk_pixbuf_get_width(pb);
  int ph = gdk_pixbuf_get_height(pb);

  GdkTexture *tex = gdk_texture_new_for_pixbuf(pb);
  GtkWidget *pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
  gtk_picture_set_can_shrink(GTK_PICTURE(pic), FALSE);
  gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
  gtk_widget_set_size_request(pic, pw, ph);
  gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
  g_object_unref(tex);
  return pic;
}

GtkWidget *logo_poli_criar(void) {
  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_add_css_class(caixa, "logo-selo");
  gtk_widget_set_margin_start(caixa, 10);
  gtk_widget_set_margin_end(caixa, 10);
  gtk_widget_set_margin_top(caixa, 4);
  gtk_widget_set_margin_bottom(caixa, 12);
  gtk_widget_set_halign(caixa, GTK_ALIGN_START);
  gtk_widget_set_valign(caixa, GTK_ALIGN_CENTER);

  GdkPixbuf *pb_poli = carregar_logo_pixbuf(
      "gui/assets/logo_poli.png", "gui/assets/logo_origem.png", 72, 165);
  GdkPixbuf *pb_upe =
      carregar_logo_pixbuf("gui/assets/logo_upe.png", NULL, 72, 125);

  if (pb_poli) {
    GtkWidget *pic_poli = criar_picture_logo(pb_poli);
    if (pic_poli)
      gtk_box_append(GTK_BOX(caixa), pic_poli);
    g_object_unref(pb_poli);
  }

  if (pb_poli && pb_upe) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_size_request(sep, 1, 44);
    gtk_widget_set_valign(sep, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(sep, 2);
    gtk_widget_set_margin_end(sep, 2);
    gtk_box_append(GTK_BOX(caixa), sep);
  }

  if (pb_upe) {
    GtkWidget *pic_upe = criar_picture_logo(pb_upe);
    if (pic_upe)
      gtk_box_append(GTK_BOX(caixa), pic_upe);
    g_object_unref(pb_upe);
  }

  return caixa;
}
