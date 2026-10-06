#define _POSIX_C_SOURCE 200809L

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* duplicar_string(const char *s) {
    if (s == NULL) return NULL;

    char *copia = strdup(s);

    if (copia == NULL) {
        fprintf(stderr, "Erro de alocacao de memoria.\n");
        exit(1);
    }

    return copia;
}

ASTNode* criar_no(NodeType type, int linha) {
    ASTNode *no = (ASTNode*) malloc(sizeof(ASTNode));

    if (no == NULL) {
        fprintf(stderr, "Erro de alocacao de memoria.\n");
        exit(1);
    }

    no->type = type;
    no->linha = linha;

    no->esq = NULL;
    no->dir = NULL;
    no->terceiro = NULL;
    no->proximo = NULL;

    no->id_name = NULL;
    no->str_val = NULL;

    no->int_val = 0;
    no->car_val = 0;
    no->op = 0;

    no->value_type = TIPO_INVALIDO;

    no->eh_vetor = 0;
    no->tamanho_vetor = 0;

    return no;
}

ASTNode* criar_no_programa(ASTNode *conteudo, int linha) {
    ASTNode *no = criar_no(NODE_PROGRAMA, linha);
    no->esq = conteudo;
    return no;
}

ASTNode* criar_no_bloco(ASTNode *decls, ASTNode *comandos, int linha) {
    ASTNode *no = criar_no(NODE_BLOCO, linha);
    no->esq = decls;
    no->dir = comandos;
    return no;
}

ASTNode* criar_no_declaracao(char *name, ASTNode *tipo, int linha) {
    ASTNode *no = criar_no(NODE_DECLARACAO, linha);

    no->id_name = duplicar_string(name);
    no->esq = tipo;

    if (tipo != NULL) {
        no->value_type = tipo->value_type;
    }

    return no;
}

ASTNode* criar_no_declaracao_vetor(char *name, int tamanho, ASTNode *tipo, int linha) {
    ASTNode *no = criar_no_declaracao(name, tipo, linha);

    no->eh_vetor = 1;
    no->tamanho_vetor = tamanho;
    no->int_val = tamanho;

    return no;
}

ASTNode* criar_no_parametro(char *name, ASTNode *tipo, int eh_vetor, int linha) {
    ASTNode *no = criar_no(NODE_PARAMETRO, linha);

    no->id_name = duplicar_string(name);
    no->esq = tipo;
    no->eh_vetor = eh_vetor;

    if (tipo != NULL) {
        no->value_type = tipo->value_type;
    }

    if (eh_vetor) {
        no->tamanho_vetor = -1;
        no->int_val = -1;
    }

    return no;
}

ASTNode* criar_no_funcao(char *name, ASTNode *params, ASTNode *tipo, ASTNode *bloco, int linha) {
    ASTNode *no = criar_no(NODE_FUNCAO, linha);

    no->id_name = duplicar_string(name);
    no->esq = params;
    no->dir = tipo;
    no->terceiro = bloco;

    if (tipo != NULL) {
        no->value_type = tipo->value_type;
    }

    return no;
}

ASTNode* criar_no_tipo(ValueType tipo, int linha) {
    ASTNode *no = criar_no(NODE_TIPO, linha);
    no->value_type = tipo;
    return no;
}

ASTNode* criar_no_id(char *name, int linha) {
    ASTNode *no = criar_no(NODE_IDENTIFICADOR, linha);
    no->id_name = duplicar_string(name);
    return no;
}

ASTNode* criar_no_acesso_vetor(char *name, ASTNode *indice, int linha) {
    ASTNode *no = criar_no(NODE_ACESSO_VETOR, linha);

    no->id_name = duplicar_string(name);
    no->esq = indice;
    no->eh_vetor = 1;

    return no;
}

ASTNode* criar_no_chamada_funcao(char *name, ASTNode *args, int linha) {
    ASTNode *no = criar_no(NODE_CHAMADA_FUNCAO, linha);

    no->id_name = duplicar_string(name);
    no->esq = args;

    return no;
}

ASTNode* criar_no_int(int val, int linha) {
    ASTNode *no = criar_no(NODE_CONST_INT, linha);

    no->int_val = val;
    no->value_type = TIPO_INT;

    return no;
}

ASTNode* criar_no_car(char value, int linha) {
    ASTNode *no = criar_no(NODE_CONST_CAR, linha);

    no->car_val = value;
    no->value_type = TIPO_CAR;

    return no;
}

ASTNode* criar_no_string(char *text, int linha) {
    ASTNode *no = criar_no(NODE_CONST_STRING, linha);
    no->str_val = duplicar_string(text);
    return no;
}

ASTNode* criar_no_op(int op, ASTNode *esq, ASTNode *dir, int linha) {
    ASTNode *no = criar_no(NODE_OPERACAO, linha);

    no->op = op;
    no->esq = esq;
    no->dir = dir;

    return no;
}

ASTNode* criar_no_atribuicao(ASTNode *id, ASTNode *expr, int linha) {
    ASTNode *no = criar_no(NODE_ATRIBUICAO, linha);

    no->esq = id;
    no->dir = expr;

    return no;
}

ASTNode* criar_no_leia(char *name, int linha) {
    ASTNode *no = criar_no(NODE_LEIA, linha);

    no->id_name = duplicar_string(name);
    no->esq = criar_no_id(name, linha);

    return no;
}

ASTNode* criar_no_escreva(ASTNode *expr, int linha) {
    ASTNode *no = criar_no(NODE_ESCREVA, linha);
    no->esq = expr;
    return no;
}

ASTNode* criar_no_novalinha(int linha) {
    return criar_no(NODE_NOVALINHA, linha);
}

ASTNode* criar_no_if(ASTNode *cond, ASTNode *entao, ASTNode *senao, int linha) {
    ASTNode *no = criar_no(NODE_IF, linha);

    no->esq = cond;
    no->dir = entao;
    no->terceiro = senao;

    return no;
}

ASTNode* criar_no_while(ASTNode *cond, ASTNode *corpo, int linha) {
    ASTNode *no = criar_no(NODE_WHILE, linha);

    no->esq = cond;
    no->dir = corpo;

    return no;
}

ASTNode* criar_no_retorne(ASTNode *expr, int linha) {
    ASTNode *no = criar_no(NODE_RETORNE, linha);
    no->esq = expr;
    return no;
}

ASTNode* anexar_no(ASTNode *lista, ASTNode *no) {
    if (lista == NULL) return no;
    if (no == NULL) return lista;

    ASTNode *p = lista;

    while (p->proximo != NULL) {
        p = p->proximo;
    }

    p->proximo = no;

    return lista;
}

const char* nome_tipo(ValueType tipo) {
    switch (tipo) {
        case TIPO_INT:
            return "int";

        case TIPO_CAR:
            return "car";

        default:
            return "invalido";
    }
}

const char* nome_no(NodeType type) {
    switch (type) {
        case NODE_PROGRAMA: return "PROGRAMA";
        case NODE_BLOCO: return "BLOCO";

        case NODE_DECLARACAO: return "DECLARACAO";
        case NODE_TIPO: return "TIPO";
        case NODE_PARAMETRO: return "PARAMETRO";
        case NODE_FUNCAO: return "FUNCAO";

        case NODE_ATRIBUICAO: return "ATRIBUICAO";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_RETORNE: return "RETORNE";

        case NODE_OPERACAO: return "OPERACAO";
        case NODE_IDENTIFICADOR: return "IDENTIFICADOR";
        case NODE_ACESSO_VETOR: return "ACESSO_VETOR";
        case NODE_CHAMADA_FUNCAO: return "CHAMADA_FUNCAO";

        case NODE_CONST_INT: return "CONST_INT";
        case NODE_CONST_CAR: return "CONST_CAR";
        case NODE_CONST_STRING: return "CONST_STRING";

        case NODE_LEIA: return "LEIA";
        case NODE_ESCREVA: return "ESCREVA";
        case NODE_NOVALINHA: return "NOVALINHA";

        default: return "NO_DESCONHECIDO";
    }
}

static void imprimir_indentacao(int nivel) {
    int i;

    for (i = 0; i < nivel; i++) {
        printf("  ");
    }
}

static void imprimir_operador(int op) {
    switch (op) {
        case '+': printf("+"); break;
        case '-': printf("-"); break;
        case '*': printf("*"); break;
        case '/': printf("/"); break;
        case '<': printf("<"); break;
        case '>': printf(">"); break;
        case '=': printf("="); break;
        case '!': printf("!"); break;
        case '[': printf("[]"); break;
        case '(': printf("call"); break;

        default:
            printf("%d", op);
            break;
    }
}

static void imprimir_info_no(ASTNode *no) {
    switch (no->type) {
        case NODE_DECLARACAO:
            printf("DECLARACAO: %s", no->id_name ? no->id_name : "(null)");

            if (no->eh_vetor) {
                printf("[%d]", no->tamanho_vetor);
            }

            if (no->esq != NULL) {
                printf(" : %s", nome_tipo(no->esq->value_type));
            }

            printf(" (linha %d)\n", no->linha);
            break;

        case NODE_PARAMETRO:
            printf("PARAMETRO: %s", no->id_name ? no->id_name : "(null)");

            if (no->eh_vetor) {
                printf("[]");
            }

            if (no->esq != NULL) {
                printf(" : %s", nome_tipo(no->esq->value_type));
            }

            printf(" (linha %d)\n", no->linha);
            break;

        case NODE_FUNCAO:
            printf("FUNCAO: %s", no->id_name ? no->id_name : "(null)");

            if (no->dir != NULL) {
                printf(" : %s", nome_tipo(no->dir->value_type));
            }

            printf(" (linha %d)\n", no->linha);
            break;

        case NODE_TIPO:
            printf("TIPO: %s (linha %d)\n", nome_tipo(no->value_type), no->linha);
            break;

        case NODE_IDENTIFICADOR:
            printf("IDENTIFICADOR: %s (linha %d)\n",
                   no->id_name ? no->id_name : "(null)", no->linha);
            break;

        case NODE_ACESSO_VETOR:
            printf("ACESSO_VETOR: %s[] (linha %d)\n",
                   no->id_name ? no->id_name : "(null)", no->linha);
            break;

        case NODE_CHAMADA_FUNCAO:
            printf("CHAMADA_FUNCAO: %s (linha %d)\n",
                   no->id_name ? no->id_name : "(null)", no->linha);
            break;

        case NODE_CONST_INT:
            printf("CONST_INT: %d (linha %d)\n", no->int_val, no->linha);
            break;

        case NODE_CONST_CAR:
            if (no->car_val == '\n') {
                printf("CONST_CAR: '\\n' (linha %d)\n", no->linha);
            } else if (no->car_val == '\t') {
                printf("CONST_CAR: '\\t' (linha %d)\n", no->linha);
            } else if (no->car_val == '\'') {
                printf("CONST_CAR: '\\'' (linha %d)\n", no->linha);
            } else if (no->car_val == '\\') {
                printf("CONST_CAR: '\\\\' (linha %d)\n", no->linha);
            } else {
                printf("CONST_CAR: '%c' (linha %d)\n", no->car_val, no->linha);
            }
            break;

        case NODE_CONST_STRING:
            printf("CONST_STRING: %s (linha %d)\n",
                   no->str_val ? no->str_val : "(null)", no->linha);
            break;

        case NODE_OPERACAO:
            printf("OPERACAO: ");
            imprimir_operador(no->op);
            printf(" (linha %d)\n", no->linha);
            break;

        default:
            printf("%s (linha %d)\n", nome_no(no->type), no->linha);
            break;
    }
}

void imprimir_ast(ASTNode *no, int nivel) {
    if (no == NULL) return;

    imprimir_indentacao(nivel);
    imprimir_info_no(no);

    if (no->esq != NULL) {
        imprimir_indentacao(nivel + 1);

        if (no->type == NODE_FUNCAO) {
            printf("parametros:\n");
        } else if (no->type == NODE_ACESSO_VETOR) {
            printf("indice:\n");
        } else if (no->type == NODE_CHAMADA_FUNCAO) {
            printf("argumentos:\n");
        } else {
            printf("esq:\n");
        }

        imprimir_ast(no->esq, nivel + 2);
    }

    if (no->dir != NULL) {
        imprimir_indentacao(nivel + 1);

        if (no->type == NODE_FUNCAO) {
            printf("tipo_retorno:\n");
        } else {
            printf("dir:\n");
        }

        imprimir_ast(no->dir, nivel + 2);
    }

    if (no->terceiro != NULL) {
        imprimir_indentacao(nivel + 1);

        if (no->type == NODE_IF) {
            printf("senao:\n");
        } else if (no->type == NODE_FUNCAO) {
            printf("bloco:\n");
        } else {
            printf("terceiro:\n");
        }

        imprimir_ast(no->terceiro, nivel + 2);
    }

    if (no->proximo != NULL) {
        imprimir_indentacao(nivel + 1);
        printf("proximo:\n");
        imprimir_ast(no->proximo, nivel + 2);
    }
}

void liberar_ast(ASTNode *no) {
    if (no == NULL) return;

    liberar_ast(no->esq);
    liberar_ast(no->dir);
    liberar_ast(no->terceiro);
    liberar_ast(no->proximo);

    free(no->id_name);
    free(no->str_val);
    free(no);
}
