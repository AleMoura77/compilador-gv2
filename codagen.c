#define _POSIX_C_SOURCE 200809L

#include "codagen.h"
#include "g-v2.tab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarInfo {
    char *nome;
    ValueType tipo;

    int escopo;
    int posicao;

    int eh_vetor;
    int tamanho_vetor;
    int eh_parametro;

    int eh_global;
    int offset;

    struct VarInfo *prox;
} VarInfo;

typedef struct ScopeInfo {
    int escopo;
    VarInfo *variaveis;
    struct ScopeInfo *prox;
} ScopeInfo;

typedef struct StringInfo {
    ASTNode *no;
    char rotulo[64];
    struct StringInfo *prox;
} StringInfo;

typedef struct FuncInfo {
    char *nome;
    ASTNode *no;
    char rotulo[128];
    struct FuncInfo *prox;
} FuncInfo;

static FILE *out = NULL;

static ScopeInfo *pilha_escopos = NULL;
static VarInfo *variaveis_globais = NULL;
static StringInfo *lista_strings = NULL;
static FuncInfo *lista_funcoes = NULL;

static int escopo_atual = 0;
static int proxima_posicao = 1;
static int contador_label = 0;
static int contador_string = 0;

static int dentro_funcao = 0;
static int proxima_posicao_local_funcao = 1;
static int proxima_posicao_param_funcao = 1;
static int quantidade_parametros_funcao_atual = 0;
static char rotulo_retorno_atual[128];

static void erro_codegen(const char *msg) {
    fprintf(stderr, "ERRO NA GERACAO DE CODIGO: %s\n", msg);
    exit(1);
}

static char *copiar_string(const char *s) {
    char *copia;

    if (s == NULL) {
        return NULL;
    }

    copia = strdup(s);

    if (copia == NULL) {
        erro_codegen("falha de memoria");
    }

    return copia;
}

static int nova_label(void) {
    return contador_label++;
}

/* ============================================================
   Maquina de pilha com acumulador
   $s0 = acumulador
   $sp = pilha
   $t1 = auxiliar para desempilhar
   ============================================================ */

static void empilhar_s0(void) {
    fprintf(out, "    sw $s0, 0($sp)\n");
    fprintf(out, "    addiu $sp, $sp, -4\n");
}

static void desempilhar_t1(void) {
    fprintf(out, "    lw $t1, 4($sp)\n");
    fprintf(out, "    addiu $sp, $sp, 4\n");
}

/* ============================================================
   Escopos
   ============================================================ */

static void entrar_escopo(void) {
    ScopeInfo *novo = (ScopeInfo *) malloc(sizeof(ScopeInfo));

    if (novo == NULL) {
        erro_codegen("falha ao criar escopo");
    }

    escopo_atual++;

    novo->escopo = escopo_atual;
    novo->variaveis = NULL;
    novo->prox = pilha_escopos;

    pilha_escopos = novo;
}

static void sair_escopo(void) {
    ScopeInfo *remover;
    VarInfo *v;

    if (pilha_escopos == NULL) {
        return;
    }

    remover = pilha_escopos;
    v = remover->variaveis;

    while (v != NULL) {
        VarInfo *prox = v->prox;
        free(v->nome);
        free(v);
        v = prox;
    }

    pilha_escopos = remover->prox;
    free(remover);

    if (escopo_atual > 0) {
        escopo_atual--;
    }
}

static int tamanho_variavel(ASTNode *decl) {
    if (decl == NULL) {
        return 0;
    }

    if (decl->eh_vetor) {
        if (decl->tamanho_vetor > 0) {
            return decl->tamanho_vetor;
        }

        return 1;
    }

    return 1;
}

static int offset_variavel(VarInfo *v) {
    return v->offset;
}

static VarInfo *criar_var_info(ASTNode *decl, int eh_global) {
    VarInfo *v = (VarInfo *) malloc(sizeof(VarInfo));

    if (v == NULL) {
        erro_codegen("falha ao criar variavel");
    }

    v->nome = copiar_string(decl->id_name);
    v->tipo = decl->value_type;

    if (decl->esq != NULL && decl->esq->type == NODE_TIPO) {
        v->tipo = decl->esq->value_type;
    }

    v->escopo = escopo_atual;

    v->eh_vetor = decl->eh_vetor;
    v->tamanho_vetor = decl->tamanho_vetor;
    v->eh_parametro = (decl->type == NODE_PARAMETRO);
    v->eh_global = eh_global;

    if (eh_global) {
        v->posicao = proxima_posicao;
        v->offset = -((v->posicao - 1) * 4);
        proxima_posicao += tamanho_variavel(decl);
    } else if (dentro_funcao && v->eh_parametro) {
        /*
         * Layout da chamada conforme os slides:
         *   0($fp)  -> $ra
         *   4($fp)  -> parametro 1
         *   8($fp)  -> parametro 2
         *   ...
         *   4*(n+1)($fp) -> $fp antigo do chamador
         * Parametro vetor ocupa uma palavra: o endereco base do vetor.
         */
        v->posicao = proxima_posicao_param_funcao;
        v->offset = 4 * v->posicao;
        proxima_posicao_param_funcao += 1;
    } else if (dentro_funcao) {
        /*
         * Variaveis locais da funcao ficam abaixo do $fp.
         *   -4($fp)  -> local 1
         *   -8($fp)  -> local 2
         */
        v->posicao = proxima_posicao_local_funcao;
        v->offset = -(4 * v->posicao);
        proxima_posicao_local_funcao += tamanho_variavel(decl);
    } else {
        /* Bloco principal: o $fp aponta para a primeira variavel local. */
        v->posicao = proxima_posicao;
        v->offset = -((v->posicao - 1) * 4);
        proxima_posicao += tamanho_variavel(decl);
    }

    v->prox = NULL;

    return v;
}

static void inserir_variavel_escopo(ASTNode *decl, int eh_global) {
    VarInfo *v;

    if (decl == NULL) {
        return;
    }

    v = criar_var_info(decl, eh_global);

    if (eh_global) {
        v->prox = variaveis_globais;
        variaveis_globais = v;
        return;
    }

    if (pilha_escopos == NULL) {
        erro_codegen("nao ha escopo para inserir variavel");
    }

    v->prox = pilha_escopos->variaveis;
    pilha_escopos->variaveis = v;
}

static VarInfo *buscar_variavel_local(const char *nome) {
    ScopeInfo *escopo = pilha_escopos;

    while (escopo != NULL) {
        VarInfo *v = escopo->variaveis;

        while (v != NULL) {
            if (strcmp(v->nome, nome) == 0) {
                return v;
            }

            v = v->prox;
        }

        escopo = escopo->prox;
    }

    return NULL;
}

static VarInfo *buscar_variavel_global(const char *nome) {
    VarInfo *v = variaveis_globais;

    while (v != NULL) {
        if (strcmp(v->nome, nome) == 0) {
            return v;
        }

        v = v->prox;
    }

    return NULL;
}

static VarInfo *buscar_variavel(const char *nome) {
    VarInfo *v = buscar_variavel_local(nome);

    if (v != NULL) {
        return v;
    }

    return buscar_variavel_global(nome);
}

static const char *base_variavel(VarInfo *v) {
    if (v->eh_global) {
        return "$s1";
    }

    return "$fp";
}

static int eh_parametro_vetor(VarInfo *v) {
    return v != NULL && v->eh_parametro && v->eh_vetor;
}

static void gerar_endereco_base_vetor(VarInfo *v) {
    int offset;

    if (v == NULL || !v->eh_vetor) {
        erro_codegen("identificador nao e vetor");
    }

    offset = offset_variavel(v);

    if (eh_parametro_vetor(v)) {
        fprintf(out, "    lw $s0, %d($fp)\n", offset);
    } else {
        fprintf(out, "    addiu $s0, %s, %d\n", base_variavel(v), offset);
    }
}

/* ============================================================
   Strings e funcoes
   ============================================================ */

static StringInfo *buscar_string(ASTNode *no) {
    StringInfo *s = lista_strings;

    while (s != NULL) {
        if (s->no == no) {
            return s;
        }

        s = s->prox;
    }

    return NULL;
}

static StringInfo *adicionar_string(ASTNode *no) {
    StringInfo *s;

    s = buscar_string(no);

    if (s != NULL) {
        return s;
    }

    s = (StringInfo *) malloc(sizeof(StringInfo));

    if (s == NULL) {
        erro_codegen("falha ao criar string");
    }

    s->no = no;
    snprintf(s->rotulo, sizeof(s->rotulo), "str_%d", contador_string++);

    s->prox = lista_strings;
    lista_strings = s;

    return s;
}

static FuncInfo *buscar_funcao(const char *nome) {
    FuncInfo *f = lista_funcoes;

    while (f != NULL) {
        if (strcmp(f->nome, nome) == 0) {
            return f;
        }

        f = f->prox;
    }

    return NULL;
}

static void adicionar_funcao(ASTNode *no) {
    FuncInfo *f;

    if (no == NULL || no->type != NODE_FUNCAO) {
        return;
    }

    if (buscar_funcao(no->id_name) != NULL) {
        return;
    }

    f = (FuncInfo *) malloc(sizeof(FuncInfo));

    if (f == NULL) {
        erro_codegen("falha ao criar funcao");
    }

    f->nome = copiar_string(no->id_name);
    f->no = no;
    snprintf(f->rotulo, sizeof(f->rotulo), "func_%s", no->id_name);

    f->prox = lista_funcoes;
    lista_funcoes = f;
}

static void coletar_strings_e_funcoes(ASTNode *no) {
    while (no != NULL) {
        if (no->type == NODE_CONST_STRING) {
            adicionar_string(no);
        } else if (no->type == NODE_FUNCAO) {
            adicionar_funcao(no);
        }

        coletar_strings_e_funcoes(no->esq);
        coletar_strings_e_funcoes(no->dir);
        coletar_strings_e_funcoes(no->terceiro);

        no = no->proximo;
    }
}

/* ============================================================
   Declaracoes
   ============================================================ */

static int contar_espaco_declaracoes(ASTNode *decls) {
    int total = 0;

    while (decls != NULL) {
        if (decls->type == NODE_DECLARACAO || decls->type == NODE_PARAMETRO) {
            total += tamanho_variavel(decls);
        }

        decls = decls->proximo;
    }

    return total;
}

static void inserir_declaracoes(ASTNode *decls, int eh_global) {
    while (decls != NULL) {
        if (decls->type == NODE_DECLARACAO || decls->type == NODE_PARAMETRO) {
            inserir_variavel_escopo(decls, eh_global);
        }

        decls = decls->proximo;
    }
}

static void alocar_espaco(int n) {
    if (n > 0) {
        fprintf(out, "    addiu $sp, $sp, -%d\n", n * 4);
    }
}

static void desalocar_espaco(int n) {
    if (n > 0) {
        fprintf(out, "    addiu $sp, $sp, %d\n", n * 4);
    }
}

/* ============================================================
   Endereco de lvalue
   Resultado: endereco fica em $s0
   ============================================================ */

static void gerar_expr(ASTNode *no);

static void gerar_endereco_lvalue(ASTNode *no) {
    VarInfo *v;
    int offset;

    if (no == NULL) {
        erro_codegen("lvalue nulo");
    }

    if (no->type == NODE_IDENTIFICADOR) {
        v = buscar_variavel(no->id_name);

        if (v == NULL) {
            erro_codegen("variavel nao encontrada");
        }

        offset = offset_variavel(v);

        fprintf(out, "    addiu $s0, %s, %d\n", base_variavel(v), offset);
        return;
    }

    if (no->type == NODE_ACESSO_VETOR) {
        v = buscar_variavel(no->id_name);

        if (v == NULL) {
            erro_codegen("vetor nao encontrado");
        }

        gerar_expr(no->esq);
        empilhar_s0();

        gerar_endereco_base_vetor(v);

        desempilhar_t1();

        fprintf(out, "    sll $t1, $t1, 2\n");
        fprintf(out, "    subu $s0, $s0, $t1\n");
        return;
    }

    erro_codegen("lvalue invalido");
}

/* ============================================================
   Expressoes
   cgenEx(e): deixa resultado em $s0 e preserva a pilha.
   ============================================================ */

static void gerar_operacao_binaria(ASTNode *no) {
    gerar_expr(no->esq);

    empilhar_s0();

    gerar_expr(no->dir);

    desempilhar_t1();

    switch (no->op) {
        case '+':
            fprintf(out, "    add $s0, $t1, $s0\n");
            break;

        case '-':
            fprintf(out, "    sub $s0, $t1, $s0\n");
            break;

        case '*':
            fprintf(out, "    mul $s0, $t1, $s0\n");
            break;

        case '/':
            fprintf(out, "    div $t1, $s0\n");
            fprintf(out, "    mflo $s0\n");
            break;

        case '<':
            fprintf(out, "    slt $s0, $t1, $s0\n");
            break;

        case '>':
            fprintf(out, "    sgt $s0, $t1, $s0\n");
            break;

        case MAIORIGUAL:
            fprintf(out, "    sge $s0, $t1, $s0\n");
            break;

        case MENORIGUAL:
            fprintf(out, "    sle $s0, $t1, $s0\n");
            break;

        case IGUAL:
            fprintf(out, "    seq $s0, $t1, $s0\n");
            break;

        case DIFERENTE:
            fprintf(out, "    sne $s0, $t1, $s0\n");
            break;

        case E:
            fprintf(out, "    sne $t1, $t1, $zero\n");
            fprintf(out, "    sne $s0, $s0, $zero\n");
            fprintf(out, "    and $s0, $t1, $s0\n");
            break;

        case OU:
            fprintf(out, "    sne $t1, $t1, $zero\n");
            fprintf(out, "    sne $s0, $s0, $zero\n");
            fprintf(out, "    or $s0, $t1, $s0\n");
            break;

        default:
            erro_codegen("operador binario desconhecido");
    }
}

static int contar_nos_lista(ASTNode *lista) {
    int n = 0;

    while (lista != NULL) {
        n++;
        lista = lista->proximo;
    }

    return n;
}

static void preencher_vetor_lista(ASTNode *lista, ASTNode **vetor, int n) {
    int i = 0;

    while (lista != NULL && i < n) {
        vetor[i++] = lista;
        lista = lista->proximo;
    }
}

static void gerar_chamada_funcao(ASTNode *no) {
    FuncInfo *f = buscar_funcao(no->id_name);
    ASTNode **args;
    ASTNode **formais;
    int n_args;
    int n_formais;
    int i;

    if (f == NULL) {
        erro_codegen("funcao nao encontrada");
    }

    n_args = contar_nos_lista(no->esq);
    n_formais = contar_nos_lista(f->no->esq);

    if (n_args != n_formais) {
        erro_codegen("numero de argumentos incompativel na chamada de funcao");
    }

    args = NULL;
    formais = NULL;

    if (n_args > 0) {
        args = (ASTNode **) malloc(sizeof(ASTNode *) * n_args);
        formais = (ASTNode **) malloc(sizeof(ASTNode *) * n_formais);

        if (args == NULL || formais == NULL) {
            erro_codegen("falha de memoria em chamada de funcao");
        }

        preencher_vetor_lista(no->esq, args, n_args);
        preencher_vetor_lista(f->no->esq, formais, n_formais);
    }

    fprintf(out, "    sw $fp, 0($sp)\n");
    fprintf(out, "    addiu $sp, $sp, -4\n");

    for (i = n_args - 1; i >= 0; i--) {
        int deve_passar_endereco = 0;

        if (formais[i] != NULL && formais[i]->type == NODE_PARAMETRO && formais[i]->eh_vetor) {
            deve_passar_endereco = 1;
        }

        if (deve_passar_endereco) {
            VarInfo *varg;

            if (args[i]->type != NODE_IDENTIFICADOR) {
                erro_codegen("parametro vetor deve receber identificador de vetor");
            }

            varg = buscar_variavel(args[i]->id_name);

            if (varg == NULL || !varg->eh_vetor) {
                erro_codegen("argumento vetor invalido");
            }

            gerar_endereco_base_vetor(varg);
        } else {
            gerar_expr(args[i]);
        }

        empilhar_s0();
    }

    fprintf(out, "    jal %s\n", f->rotulo);

    free(args);
    free(formais);
}

static void gerar_expr(ASTNode *no) {
    VarInfo *v;
    int offset;

    if (no == NULL) {
        erro_codegen("expressao nula");
    }

    switch (no->type) {
        case NODE_CONST_INT:
            fprintf(out, "    li $s0, %d\n", no->int_val);
            break;

        case NODE_CONST_CAR:
            fprintf(out, "    li $s0, %d\n", (int) no->car_val);
            break;

        case NODE_IDENTIFICADOR:
            v = buscar_variavel(no->id_name);

            if (v == NULL) {
                erro_codegen("variavel nao encontrada");
            }

            if (v->eh_vetor) {
                gerar_endereco_base_vetor(v);
            } else {
                offset = offset_variavel(v);
                fprintf(out, "    lw $s0, %d(%s)\n", offset, base_variavel(v));
            }
            break;

        case NODE_ACESSO_VETOR:
            gerar_endereco_lvalue(no);
            fprintf(out, "    lw $s0, 0($s0)\n");
            break;

        case NODE_ATRIBUICAO:
            gerar_endereco_lvalue(no->esq);
            empilhar_s0();

            gerar_expr(no->dir);

            desempilhar_t1();

            fprintf(out, "    sw $s0, 0($t1)\n");
            break;

        case NODE_CHAMADA_FUNCAO:
            gerar_chamada_funcao(no);
            break;

        case NODE_OPERACAO:
            if (no->op == '!' && no->dir != NULL) {
                gerar_expr(no->dir);
                fprintf(out, "    seq $s0, $s0, $zero\n");
            } else if (no->op == '-' && no->esq == NULL) {
                gerar_expr(no->dir);
                fprintf(out, "    sub $s0, $zero, $s0\n");
            } else {
                gerar_operacao_binaria(no);
            }
            break;

        default:
            erro_codegen("tipo de expressao nao suportado");
    }
}

/* ============================================================
   Comandos
   ============================================================ */

static void gerar_comando(ASTNode *no);

static void gerar_bloco(ASTNode *bloco) {
    int espaco;
    int posicao_antes;

    if (bloco == NULL || bloco->type != NODE_BLOCO) {
        return;
    }

    posicao_antes = dentro_funcao ? proxima_posicao_local_funcao : proxima_posicao;

    entrar_escopo();

    espaco = contar_espaco_declaracoes(bloco->esq);

    inserir_declaracoes(bloco->esq, 0);
    alocar_espaco(espaco);

    gerar_comando(bloco->dir);

    desalocar_espaco(espaco);

    sair_escopo();

    if (dentro_funcao) {
        proxima_posicao_local_funcao = posicao_antes;
    } else {
        proxima_posicao = posicao_antes;
    }
}

static void gerar_leia(ASTNode *no) {
    ValueType tipo = TIPO_INT;

    if (no->esq == NULL) {
        return;
    }

    tipo = no->esq->value_type;

    if (tipo == TIPO_CAR) {
        fprintf(out, "    li $v0, 12\n");
    } else {
        fprintf(out, "    li $v0, 5\n");
    }

    fprintf(out, "    syscall\n");

    gerar_endereco_lvalue(no->esq);

    fprintf(out, "    sw $v0, 0($s0)\n");
}

static void gerar_escreva(ASTNode *no) {
    StringInfo *s;

    if (no->esq == NULL) {
        return;
    }

    if (no->esq->type == NODE_CONST_STRING) {
        s = adicionar_string(no->esq);

        fprintf(out, "    li $v0, 4\n");
        fprintf(out, "    la $a0, %s\n", s->rotulo);
        fprintf(out, "    syscall\n");
        return;
    }

    gerar_expr(no->esq);

    if (no->esq->value_type == TIPO_CAR) {
        fprintf(out, "    li $v0, 11\n");
    } else {
        fprintf(out, "    li $v0, 1\n");
    }

    fprintf(out, "    move $a0, $s0\n");
    fprintf(out, "    syscall\n");
}

static void gerar_if(ASTNode *no) {
    int l_else = nova_label();
    int l_fim = nova_label();

    gerar_expr(no->esq);

    fprintf(out, "    beq $s0, $zero, L%d\n", l_else);

    gerar_comando(no->dir);

    fprintf(out, "    b L%d\n", l_fim);

    fprintf(out, "L%d:\n", l_else);

    if (no->terceiro != NULL) {
        gerar_comando(no->terceiro);
    }

    fprintf(out, "L%d:\n", l_fim);
}

static void gerar_while(ASTNode *no) {
    int l_inicio = nova_label();
    int l_fim = nova_label();

    fprintf(out, "L%d:\n", l_inicio);

    gerar_expr(no->esq);

    fprintf(out, "    beq $s0, $zero, L%d\n", l_fim);

    gerar_comando(no->dir);

    fprintf(out, "    b L%d\n", l_inicio);

    fprintf(out, "L%d:\n", l_fim);
}

static void gerar_retorne(ASTNode *no) {
    if (!dentro_funcao) {
        return;
    }

    if (no->esq != NULL) {
        gerar_expr(no->esq);
    }

    fprintf(out, "    b %s\n", rotulo_retorno_atual);
}

static void gerar_comando(ASTNode *no) {
    while (no != NULL) {
        switch (no->type) {
            case NODE_BLOCO:
                gerar_bloco(no);
                break;

            case NODE_ATRIBUICAO:
            case NODE_OPERACAO:
            case NODE_CHAMADA_FUNCAO:
                gerar_expr(no);
                break;

            case NODE_LEIA:
                gerar_leia(no);
                break;

            case NODE_ESCREVA:
                gerar_escreva(no);
                break;

            case NODE_NOVALINHA:
                fprintf(out, "    li $v0, 4\n");
                fprintf(out, "    la $a0, newline\n");
                fprintf(out, "    syscall\n");
                break;

            case NODE_IF:
                gerar_if(no);
                break;

            case NODE_WHILE:
                gerar_while(no);
                break;

            case NODE_RETORNE:
                gerar_retorne(no);
                break;

            default:
                break;
        }

        no = no->proximo;
    }
}

/* ============================================================
   Funcoes da G-V2
   ============================================================ */

static int contar_parametros(ASTNode *params) {
    int n = 0;

    while (params != NULL) {
        if (params->type == NODE_PARAMETRO) {
            n += 1;
        }

        params = params->proximo;
    }

    return n;
}

static void inserir_parametros_funcao(ASTNode *params) {
    while (params != NULL) {
        if (params->type == NODE_PARAMETRO) {
            inserir_variavel_escopo(params, 0);
        }

        params = params->proximo;
    }
}

static void gerar_funcao(ASTNode *funcao) {
    FuncInfo *f;
    int espaco_params;
    int espaco_decls;
    int id_ret;
    int posicao_antes;
    int posicao_local_antes;
    int posicao_param_antes;
    int qtd_param_antes;
    int dentro_funcao_antes;

    if (funcao == NULL || funcao->type != NODE_FUNCAO) {
        return;
    }

    f = buscar_funcao(funcao->id_name);

    if (f == NULL) {
        erro_codegen("funcao nao encontrada na lista");
    }

    id_ret = nova_label();

    snprintf(rotulo_retorno_atual, sizeof(rotulo_retorno_atual),
             "Lret_%s_%d", funcao->id_name, id_ret);

    fprintf(out, "%s:\n", f->rotulo);

    fprintf(out, "    move $fp, $sp\n");
    fprintf(out, "    sw $ra, 0($sp)\n");
    fprintf(out, "    addiu $sp, $sp, -4\n");

    dentro_funcao_antes = dentro_funcao;
    posicao_antes = proxima_posicao;
    posicao_local_antes = proxima_posicao_local_funcao;
    posicao_param_antes = proxima_posicao_param_funcao;
    qtd_param_antes = quantidade_parametros_funcao_atual;

    dentro_funcao = 1;
    proxima_posicao_local_funcao = 1;
    proxima_posicao_param_funcao = 1;

    entrar_escopo();

    espaco_params = contar_parametros(funcao->esq);
    quantidade_parametros_funcao_atual = espaco_params;
    inserir_parametros_funcao(funcao->esq);

    espaco_decls = 0;

    if (funcao->terceiro != NULL && funcao->terceiro->type == NODE_BLOCO) {
        espaco_decls = contar_espaco_declaracoes(funcao->terceiro->esq);
        inserir_declaracoes(funcao->terceiro->esq, 0);
        alocar_espaco(espaco_decls);
        gerar_comando(funcao->terceiro->dir);
    }

    fprintf(out, "%s:\n", rotulo_retorno_atual);

    sair_escopo();

    fprintf(out, "    lw $ra, 0($fp)\n");
    fprintf(out, "    move $sp, $fp\n");
    fprintf(out, "    addiu $sp, $sp, %d\n", (espaco_params + 1) * 4);
    fprintf(out, "    lw $fp, 0($sp)\n");
    fprintf(out, "    jr $ra\n\n");

    dentro_funcao = dentro_funcao_antes;
    proxima_posicao = posicao_antes;
    proxima_posicao_local_funcao = posicao_local_antes;
    proxima_posicao_param_funcao = posicao_param_antes;
    quantidade_parametros_funcao_atual = qtd_param_antes;
}

static void gerar_funcoes(ASTNode *lista) {
    while (lista != NULL) {
        if (lista->type == NODE_FUNCAO) {
            gerar_funcao(lista);
        }

        lista = lista->proximo;
    }
}

/* ============================================================
   Programa
   ============================================================ */

static ASTNode *encontrar_principal(ASTNode *lista) {
    while (lista != NULL) {
        if (lista->type == NODE_BLOCO) {
            return lista;
        }

        lista = lista->proximo;
    }

    return NULL;
}

static void emitir_data(void) {
    StringInfo *s;

    fprintf(out, ".data\n");
    fprintf(out, "newline: .asciiz \"\\n\"\n");

    s = lista_strings;

    while (s != NULL) {
        fprintf(out, "%s: .asciiz %s\n", s->rotulo, s->no->str_val);
        s = s->prox;
    }

    fprintf(out, "\n");
}

static void inserir_globais(ASTNode *lista) {
    ASTNode *p = lista;

    while (p != NULL) {
        if (p->type == NODE_DECLARACAO) {
            inserir_variavel_escopo(p, 1);
        }

        p = p->proximo;
    }
}

static int contar_globais(ASTNode *lista) {
    int total = 0;
    ASTNode *p = lista;

    while (p != NULL) {
        if (p->type == NODE_DECLARACAO) {
            total += tamanho_variavel(p);
        }

        p = p->proximo;
    }

    return total;
}

static void gerar_principal(ASTNode *lista) {
    ASTNode *principal;
    int espaco_global;

    principal = encontrar_principal(lista);

    fprintf(out, ".text\n");
    fprintf(out, ".globl main\n\n");

    fprintf(out, "    j main\n\n");

    proxima_posicao = 1;
    escopo_atual = 0;
    pilha_escopos = NULL;
    variaveis_globais = NULL;

    inserir_globais(lista);
    espaco_global = contar_globais(lista);

    gerar_funcoes(lista);

    fprintf(out, "main:\n");

    fprintf(out, "    li $sp, 0x7fffeffc\n");

    fprintf(out, "    move $s1, $sp\n");

    alocar_espaco(espaco_global);

    fprintf(out, "    move $fp, $sp\n");

    proxima_posicao = 1;

    if (principal != NULL) {
        gerar_bloco(principal);
    }

    fprintf(out, "    li $v0, 10\n");
    fprintf(out, "    syscall\n");

    sair_escopo();
}

void gerar_codigo(ASTNode *raiz, const char *arquivo_saida) {
    if (raiz == NULL || raiz->type != NODE_PROGRAMA) {
        erro_codegen("raiz invalida");
    }

    out = fopen(arquivo_saida, "w");

    if (out == NULL) {
        perror("Erro ao criar arquivo de saida");
        exit(1);
    }

    contador_label = 0;
    contador_string = 0;
    escopo_atual = 0;
    proxima_posicao = 1;
    dentro_funcao = 0;

    lista_strings = NULL;
    lista_funcoes = NULL;
    pilha_escopos = NULL;
    variaveis_globais = NULL;

    coletar_strings_e_funcoes(raiz);

    emitir_data();
    gerar_principal(raiz->esq);

    fclose(out);
}
