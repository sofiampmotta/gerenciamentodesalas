#include "interface.h"

typedef struct {
    int data;                           /* AAAAMMDD */
    GtkWidget *botao, *calendario, *popover;
    void (*ao_mudar)(gpointer);
    gpointer dados;
    gboolean ajustando;
} Seletor;

static const char *DIAS_CURTOS[] = { "", "seg", "ter", "qua", "qui", "sex", "sáb", "dom" };

static void atualizar_texto(Seletor *s) {
    char texto[40], d[16];
    dados_data_texto(s->data, d, sizeof d);
    GDate *g = g_date_new_dmy(s->data % 100, (s->data / 100) % 100, s->data / 10000);
    g_snprintf(texto, sizeof texto, "%s, %s", DIAS_CURTOS[g_date_get_weekday(g)], d);
    g_date_free(g);
    gtk_menu_button_set_label(GTK_MENU_BUTTON(s->botao), texto);
}

static void mostrar_no_calendario(Seletor *s) {
    GDateTime *d = g_date_time_new_local(s->data / 10000, (s->data / 100) % 100, s->data % 100, 12, 0, 0);
    if (!d) return;
    s->ajustando = TRUE;
    gtk_calendar_select_day(GTK_CALENDAR(s->calendario), d);
    s->ajustando = FALSE;
    g_date_time_unref(d);
}

static void ao_dia(GtkCalendar *cal, gpointer dados) {
    Seletor *s = (Seletor *) dados;
    if (s->ajustando) return;
    GDateTime *d = gtk_calendar_get_date(cal);
    int novo = g_date_time_get_year(d) * 10000 + g_date_time_get_month(d) * 100
             + g_date_time_get_day_of_month(d);
    g_date_time_unref(d);
    if (novo == s->data) return;

    gboolean mesmo_mes = (novo / 100) == (s->data / 100);
    s->data = novo;
    atualizar_texto(s);
    if (mesmo_mes) gtk_popover_popdown(GTK_POPOVER(s->popover));
    if (s->ao_mudar) s->ao_mudar(s->dados);
}

GtkWidget *seletor_data_criar(int data, void (*ao_mudar)(gpointer), gpointer dados) {
    Seletor *s = g_new0(Seletor, 1);
    s->data = dados_data_valida(data) ? data : DATA_PADRAO;
    s->ao_mudar = ao_mudar;
    s->dados = dados;

    s->botao = gtk_menu_button_new();
    gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(s->botao), TRUE);
    s->calendario = gtk_calendar_new();
    s->popover = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(s->popover), s->calendario);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(s->botao), s->popover);

    mostrar_no_calendario(s);
    atualizar_texto(s);
    g_signal_connect(s->calendario, "day-selected", G_CALLBACK(ao_dia), s);
    g_object_set_data_full(G_OBJECT(s->botao), "seletor", s, g_free);
    return s->botao;
}

int seletor_data_get(GtkWidget *w) {
    Seletor *s = g_object_get_data(G_OBJECT(w), "seletor");
    return s ? s->data : DATA_PADRAO;
}

void seletor_data_set(GtkWidget *w, int data) {
    Seletor *s = g_object_get_data(G_OBJECT(w), "seletor");
    if (!s || !dados_data_valida(data)) return;
    s->data = data;
    mostrar_no_calendario(s);
    atualizar_texto(s);
}
