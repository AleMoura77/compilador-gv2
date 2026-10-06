#define _POSIX_C_SOURCE 200809L

#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copiar_string(const char *s) {
    char *copia;

    if (s == NULL) {
        return NULL;
    }

    copia = strdup(s);

    if (copia == NULL) {
        fprintf(stderr, "Erro de memoria na tabela de simbolos.\n");
        exit(1);
    }

    return copia;
}

static Symbol *criar_simbolo(
    const char *nome,
    SymbolKind categoria,
    ValueType tipo,
    int escopo,
    int posicao,
    int linha,
    int eh_vetor,
    int tamanho_vetor
) {
    Symbol *s = (Symbol *) malloc(sizeof(Symbol));

    if (s == NULL) {
        fprintf(stderr, "Erro de memoria na tabela de simbolos.\n");
        exit(1);
    }

    s->nome = copiar_string(nome);
    s->categoria = categoria;
    s->tipo = tipo;
    s->escopo = escopo;
    s->posicao = posicao;
    s->linha = linha;
    s->eh_vetor = eh_vetor;
    s->tamanho_vetor = tamanho_vetor;
    s->num_parametros = 0;
    s->parametros = NULL;
    s->prox = NULL;

    return s;
}

static void liberar_simbolo(Symbol *s) {
    if (s == NULL) {
        return;
    }

    free(s->nome);
    symtab_free_param_info(s->parametros);
    free(s);
}

void symtab_init(SymbolTableStack *pilha) {
    if (pilha == NULL) {
        return;
    }

    pilha->topo = NULL;
    pilha->escopo_atual = 0;
}

void symtab_push_scope(SymbolTableStack *pilha) {
    ScopeTable *novo;

    if (pilha == NULL) {
        return;
    }

    novo = (ScopeTable *) malloc(sizeof(ScopeTable));

    if (novo == NULL) {
        fprintf(stderr, "Erro de memoria ao criar escopo.\n");
        exit(1);
    }

    pilha->escopo_atual++;

    novo->numero_escopo = pilha->escopo_atual;
    novo->simbolos = NULL;
    novo->prox = pilha->topo;

    pilha->topo = novo;
}

void symtab_pop_scope(SymbolTableStack *pilha) {
    ScopeTable *remover;
    Symbol *s;

    if (pilha == NULL || pilha->topo == NULL) {
        return;
    }

    remover = pilha->topo;
    s = remover->simbolos;

    while (s != NULL) {
        Symbol *prox = s->prox;
        liberar_simbolo(s);
        s = prox;
    }

    pilha->topo = remover->prox;
    free(remover);

    if (pilha->escopo_atual > 0) {
        pilha->escopo_atual--;
    }
}

void symtab_free(SymbolTableStack *pilha) {
    if (pilha == NULL) {
        return;
    }

    while (pilha->topo != NULL) {
        symtab_pop_scope(pilha);
    }

    pilha->escopo_atual = 0;
}

Symbol *symtab_lookup_current_scope(SymbolTableStack *pilha, const char *nome) {
    Symbol *s;

    if (pilha == NULL || pilha->topo == NULL || nome == NULL) {
        return NULL;
    }

    s = pilha->topo->simbolos;

    while (s != NULL) {
        if (strcmp(s->nome, nome) == 0) {
            return s;
        }

        s = s->prox;
    }

    return NULL;
}

Symbol *symtab_lookup(SymbolTableStack *pilha, const char *nome) {
    ScopeTable *escopo;

    if (pilha == NULL || nome == NULL) {
        return NULL;
    }

    escopo = pilha->topo;

    while (escopo != NULL) {
        Symbol *s = escopo->simbolos;

        while (s != NULL) {
            if (strcmp(s->nome, nome) == 0) {
                return s;
            }

            s = s->prox;
        }

        escopo = escopo->prox;
    }

    return NULL;
}

static int inserir_no_escopo_atual(SymbolTableStack *pilha, Symbol *novo) {
    if (pilha == NULL || pilha->topo == NULL || novo == NULL) {
        return 0;
    }

    if (symtab_lookup_current_scope(pilha, novo->nome) != NULL) {
        liberar_simbolo(novo);
        return 0;
    }

    novo->prox = pilha->topo->simbolos;
    pilha->topo->simbolos = novo;

    return 1;
}

int symtab_insert_variable(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo,
    int posicao,
    int linha,
    int eh_vetor,
    int tamanho_vetor
) {
    Symbol *novo;

    if (pilha == NULL || pilha->topo == NULL) {
        return 0;
    }

    novo = criar_simbolo(
        nome,
        SYM_VARIAVEL,
        tipo,
        pilha->topo->numero_escopo,
        posicao,
        linha,
        eh_vetor,
        tamanho_vetor
    );

    return inserir_no_escopo_atual(pilha, novo);
}

int symtab_insert_parameter(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo,
    int posicao,
    int linha,
    int eh_vetor
) {
    Symbol *novo;

    if (pilha == NULL || pilha->topo == NULL) {
        return 0;
    }

    novo = criar_simbolo(
        nome,
        SYM_PARAMETRO,
        tipo,
        pilha->topo->numero_escopo,
        posicao,
        linha,
        eh_vetor,
        -1
    );

    return inserir_no_escopo_atual(pilha, novo);
}

int symtab_insert_function(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo_retorno,
    int num_parametros,
    ParamInfo *parametros,
    int linha
) {
    Symbol *novo;

    if (pilha == NULL || pilha->topo == NULL) {
        return 0;
    }

    novo = criar_simbolo(
        nome,
        SYM_FUNCAO,
        tipo_retorno,
        pilha->topo->numero_escopo,
        0,
        linha,
        0,
        0
    );

    novo->num_parametros = num_parametros;
    novo->parametros = parametros;

    return inserir_no_escopo_atual(pilha, novo);
}

ParamInfo *symtab_create_param_info(
    const char *nome,
    ValueType tipo,
    int eh_vetor,
    int posicao
) {
    ParamInfo *p = (ParamInfo *) malloc(sizeof(ParamInfo));

    if (p == NULL) {
        fprintf(stderr, "Erro de memoria ao criar parametro.\n");
        exit(1);
    }

    p->nome = copiar_string(nome);
    p->tipo = tipo;
    p->eh_vetor = eh_vetor;
    p->posicao = posicao;
    p->prox = NULL;

    return p;
}

ParamInfo *symtab_append_param_info(ParamInfo *lista, ParamInfo *novo) {
    ParamInfo *p;

    if (lista == NULL) {
        return novo;
    }

    if (novo == NULL) {
        return lista;
    }

    p = lista;

    while (p->prox != NULL) {
        p = p->prox;
    }

    p->prox = novo;

    return lista;
}

void symtab_free_param_info(ParamInfo *lista) {
    ParamInfo *p = lista;

    while (p != NULL) {
        ParamInfo *prox = p->prox;

        free(p->nome);
        free(p);

        p = prox;
    }
}

int symtab_insert(SymbolTableStack *pilha, const char *nome, ValueType tipo, int linha) {
    return symtab_insert_variable(pilha, nome, tipo, 0, linha, 0, 0);
}

const char *symtab_kind_name(SymbolKind categoria) {
    switch (categoria) {
        case SYM_VARIAVEL:
            return "variavel";

        case SYM_PARAMETRO:
            return "parametro";

        case SYM_FUNCAO:
            return "funcao";

        default:
            return "desconhecido";
    }
}

static void imprimir_parametros(ParamInfo *p) {
    printf("(");

    while (p != NULL) {
        printf("%s", p->nome);

        if (p->eh_vetor) {
            printf("[]");
        }

        printf(":%s", nome_tipo(p->tipo));

        if (p->prox != NULL) {
            printf(", ");
        }

        p = p->prox;
    }

    printf(")");
}

void symtab_print(SymbolTableStack *pilha) {
    ScopeTable *escopo;

    if (pilha == NULL) {
        return;
    }

    printf("\n=== PILHA DE TABELAS DE SIMBOLOS ===\n");

    escopo = pilha->topo;

    while (escopo != NULL) {
        Symbol *s = escopo->simbolos;

        printf("Escopo %d:\n", escopo->numero_escopo);

        while (s != NULL) {
            printf("  %s <", s->nome);
            printf("categoria=%s, ", symtab_kind_name(s->categoria));
            printf("tipo=%s, ", nome_tipo(s->tipo));
            printf("escopo=%d, ", s->escopo);
            printf("pos=%d", s->posicao);

            if (s->eh_vetor) {
                if (s->tamanho_vetor > 0) {
                    printf(", vetor[%d]", s->tamanho_vetor);
                } else {
                    printf(", vetor[]");
                }
            }

            if (s->categoria == SYM_FUNCAO) {
                printf(", num_parametros=%d, parametros=", s->num_parametros);
                imprimir_parametros(s->parametros);
            }

            printf(">\n");

            s = s->prox;
        }

        escopo = escopo->prox;
    }

    printf("====================================\n");
}
