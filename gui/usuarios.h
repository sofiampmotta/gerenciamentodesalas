#ifndef USUARIOS_H
#define USUARIOS_H
#include <glib.h>

typedef struct {
    char login[32];
    char nome[64];
    char setor[32];
} Usuario;

int usuarios_existe_algum(void);
/* retorno: 0 = ok, -1 = erro (mensagem pronta em msg) */
int usuarios_cadastrar(const char *login, const char *nome, const char *setor,
                       const char *senha, char *msg, size_t tam);
int usuarios_entrar(const char *login, const char *senha, Usuario *u, char *msg, size_t tam);
#endif
