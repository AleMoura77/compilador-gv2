#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

typedef enum {
    SYM_VARIAVEL,
    SYM_PARAMETRO,
    SYM_FUNCAO
} SymbolKind;

typedef struct ParamInfo {
    char *nome;
    ValueType tipo;
    int eh_vetor;
    int posicao;
    struct ParamInfo *prox;
} ParamInfo;

typedef struct Symbol {
    char *nome;

    SymbolKind categoria;
    ValueType tipo;

    int escopo;
    int posicao;
    int linha;

    int eh_vetor;
    int tamanho_vetor;

    int num_parametros;
    ParamInfo *parametros;

    struct Symbol *prox;
} Symbol;

typedef struct ScopeTable {
    int numero_escopo;
    Symbol *simbolos;
    struct ScopeTable *prox;
} ScopeTable;

typedef struct SymbolTableStack {
    ScopeTable *topo;
    int escopo_atual;
} SymbolTableStack;

void symtab_init(SymbolTableStack *pilha);
void symtab_push_scope(SymbolTableStack *pilha);
void symtab_pop_scope(SymbolTableStack *pilha);
void symtab_free(SymbolTableStack *pilha);

Symbol *symtab_lookup(SymbolTableStack *pilha, const char *nome);
Symbol *symtab_lookup_current_scope(SymbolTableStack *pilha, const char *nome);

int symtab_insert_variable(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo,
    int posicao,
    int linha,
    int eh_vetor,
    int tamanho_vetor
);

int symtab_insert_parameter(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo,
    int posicao,
    int linha,
    int eh_vetor
);

int symtab_insert_function(
    SymbolTableStack *pilha,
    const char *nome,
    ValueType tipo_retorno,
    int num_parametros,
    ParamInfo *parametros,
    int linha
);

ParamInfo *symtab_create_param_info(
    const char *nome,
    ValueType tipo,
    int eh_vetor,
    int posicao
);

ParamInfo *symtab_append_param_info(ParamInfo *lista, ParamInfo *novo);
void symtab_free_param_info(ParamInfo *lista);

int symtab_insert(SymbolTableStack *pilha, const char *nome, ValueType tipo, int linha);

void symtab_print(SymbolTableStack *pilha);

const char *symtab_kind_name(SymbolKind categoria);

#endif
