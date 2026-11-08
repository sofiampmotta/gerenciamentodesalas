#include "usuarios.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define ARQUIVO "usuarios.csv"   /* formato: login;nome;setor;sal;hash */

static char *calcular_hash(const char *sal, const char *senha) {
    char *junto = g_strconcat(sal, senha, NULL);
    char *h = g_compute_checksum_for_string(G_CHECKSUM_SHA256, junto, -1);
    g_free(junto);
    return h;
}

static gchar **buscar(const char *login) {
    FILE *f = fopen(ARQUIVO, "r");
    if (!f) return NULL;
    char linha[512];
    while (fgets(linha, sizeof linha, f)) {
        g_strstrip(linha);
        if (linha[0] == '\0') continue;
        gchar **c = g_strsplit(linha, ";", -1);
        if (g_strv_length(c) >= 5 && strcmp(c[0], login) == 0) { fclose(f); return c; }
        g_strfreev(c);
    }
    fclose(f);
    return NULL;
}

int usuarios_existe_algum(void) {
    FILE *f = fopen(ARQUIVO, "r");
    if (!f) return 0;
    char linha[512];
    int achou = 0;
    while (fgets(linha, sizeof linha, f)) {
        g_strstrip(linha);
        if (linha[0] != '\0') { achou = 1; break; }
    }
    fclose(f);
    return achou;
}

static int login_valido(const char *s) {
    size_t n = strlen(s);
    if (n < 3 || n > 30) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char) s[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static int texto_ok(const char *s) {
    return strchr(s, ';') == NULL && strchr(s, '\n') == NULL && strchr(s, '\r') == NULL;
}

int usuarios_cadastrar(const char *login_in, const char *nome_in, const char *setor_in,
                       const char *senha, char *msg, size_t tam) {
    char *tmp = g_strstrip(g_strdup(login_in));
    char *login = g_ascii_strdown(tmp, -1);
    g_free(tmp);
    char *nome  = g_strstrip(g_strdup(nome_in));
    char *setor = g_strstrip(g_strdup(setor_in));
    int ret = -1;

    if (nome[0] == '\0')
        g_strlcpy(msg, "Informe o nome.", tam);
    else if (setor[0] == '\0')
        g_strlcpy(msg, "Informe o setor.", tam);
    else if (!texto_ok(nome) || !texto_ok(setor))
        g_strlcpy(msg, "O nome e o setor não podem ter ponto e vírgula.", tam);
    else if (!login_valido(login))
        g_strlcpy(msg, "O usuário deve ter de 3 a 30 caracteres: letras, números, ponto, hífen ou sublinhado.", tam);
    else if (strlen(senha) < 6)
        g_strlcpy(msg, "A senha precisa ter pelo menos 6 caracteres.", tam);
    else {
        gchar **ja = buscar(login);
        if (ja) {
            g_strfreev(ja);
            g_strlcpy(msg, "Já existe um usuário com esse nome de acesso.", tam);
        } else {
            FILE *f = fopen(ARQUIVO, "a");
            if (!f) {
                g_strlcpy(msg, "Não foi possível gravar o arquivo usuarios.csv.", tam);
            } else {
                char *sal  = g_strdup_printf("%08x%08x", g_random_int(), g_random_int());
                char *hash = calcular_hash(sal, senha);
                fprintf(f, "%s;%s;%s;%s;%s\n", login, nome, setor, sal, hash);
                fclose(f);
                g_free(sal); g_free(hash);
                g_strlcpy(msg, "Cadastro criado.", tam);
                ret = 0;
            }
        }
    }
    g_free(login); g_free(nome); g_free(setor);
    return ret;
}

int usuarios_entrar(const char *login_in, const char *senha, Usuario *u, char *msg, size_t tam) {
    char *tmp = g_strstrip(g_strdup(login_in));
    char *login = g_ascii_strdown(tmp, -1);
    g_free(tmp);
    int ret = -1;

    gchar **c = buscar(login);
    if (c) {
        char *h = calcular_hash(c[3], senha);
        if (strcmp(h, c[4]) == 0) {
            g_strlcpy(u->login, c[0], sizeof u->login);
            g_strlcpy(u->nome,  c[1], sizeof u->nome);
            g_strlcpy(u->setor, c[2], sizeof u->setor);
            ret = 0;
        }
        g_free(h);
        g_strfreev(c);
    }
    if (ret != 0) g_strlcpy(msg, "Usuário ou senha incorretos.", tam);
    g_free(login);
    return ret;
}
