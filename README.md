# Compilador G-V2

Compilador da linguagem G-V2 implementado em **C**, com análise léxica em **Flex**, análise sintática em **Bison**, verificação semântica e geração de **Assembly MIPS**. A linguagem utiliza palavras-chave em português e oferece variáveis, vetores, funções e estruturas de controle.

## Como funciona

```text
Código-fonte (.g)
    → Análise léxica (Flex)
    → Análise sintática (Bison) e construção da AST
    → Análise semântica e tabela de símbolos
    → Geração de Assembly MIPS (.g.s)
```

Após uma análise bem-sucedida, o compilador imprime a árvore sintática abstrata (AST) no terminal e grava o Assembly ao lado do arquivo de entrada. Ele não executa automaticamente o programa gerado.

## Recursos

- Tipos `int` e `car`, constantes inteiras e de caracteres.
- Variáveis globais e locais, com controle de escopos.
- Vetores com tamanho definido na declaração e parâmetros vetoriais.
- Funções com parâmetros, chamadas e retorno tipado.
- Atribuições e expressões aritméticas, relacionais e lógicas.
- Condicionais `se` / `entao` / `senao` / `fimse` e laços `enquanto`.
- Entrada e saída com `leia`, `escreva` e `novalinha`.
- Impressão de literais de texto e comentários `/* ... */`.
- Diagnósticos léxicos, sintáticos e semânticos, incluindo identificadores não declarados, declarações duplicadas e incompatibilidades de tipos e argumentos.

## Pré-requisitos

O Makefile utiliza GCC com suporte a C11, GNU Make, Flex, Bison e a biblioteca `libfl`. Os comandos abaixo destinam-se a um ambiente Linux; no Windows, utilize uma distribuição Linux no WSL com essas dependências.

Em Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install build-essential flex bison libfl-dev
```

## Compilar e usar

Na pasta do projeto:

```bash
make
./g-v2 examples/ola.g
```

O executável recebe o caminho do arquivo-fonte como argumento:

```bash
./g-v2 caminho/programa.g
```

Nesse caso, a saída será `caminho/programa.g.s`: o sufixo `.s` é acrescentado ao nome completo da entrada. A extensão `.g` é uma convenção; o programa não a exige.

Para remover o executável, os objetos e os arquivos gerados pelo Flex/Bison:

```bash
make clean
```

O comando `make clean` não remove os arquivos Assembly produzidos ao compilar programas G-V2.

### Executar o Assembly

A saída utiliza instruções MIPS, pseudoinstruções e serviços `syscall` de simuladores educacionais. Abra o arquivo `.g.s` em um simulador como MARS ou QtSPIM, monte e execute o programa. A geração de código não inclui slots de atraso explícitos; utilize execução sem *delayed branching*.

O arquivo `.s` não é um executável nativo do Windows/Linux e não deve ser compilado como Assembly x86. A compatibilidade com versões específicas dos simuladores ainda precisa ser validada.

## Exemplos

### Olá, mundo

Arquivo [`examples/ola.g`](examples/ola.g):

```text
principal {
    escreva "Ola, mundo!";
    novalinha;
}
```

Ao executar o Assembly, a saída esperada é `Ola, mundo!`, seguida de uma quebra de linha.

### Funções, variáveis e repetição

Arquivo [`examples/soma.g`](examples/soma.g):

```text
funcao [
    dobro(valor: int): int {
        retorne valor * 2;
    }
]

principal [i, total: int;] {
    i = 1;
    total = 0;
    enquanto (i <= 5) {
        total = total + dobro(i);
        i = i + 1;
    }
    escreva total;
    novalinha;
}
```

Compile com `./g-v2 examples/soma.g`. A saída esperada ao executar o Assembly é `30`, seguida de uma quebra de linha.

## Sintaxe em resumo

| Elemento | Exemplo |
| --- | --- |
| Programa principal | `principal { ... }` |
| Declaração local, antes das chaves | `principal [x: int;] { ... }` |
| Declaração global, antes das funções | `global [contador: int;]` |
| Vetor | `[valores[10]: int;]` |
| Parâmetro vetorial | `valores[]: int` |
| Caractere | `'a'` |
| Atribuição | `x = 10;` |
| Leitura | `leia x;` |
| Escrita | `escreva x;` ou `escreva "texto";` |
| Condicional | `se (x > 0) entao escreva x; senao escreva 0; fimse` |
| Repetição | `enquanto (x > 0) { x = x - 1; }` |
| Retorno de função | `retorne x;` |
| Comentário | `/* comentario */` |

Os operadores aritméticos são `+`, `-`, `*` e `/`; os relacionais são `<`, `>`, `<=`, `>=`, `==` e `!=`; os lógicos são `&`, `||` e `!`. O operador E lógico é escrito com **um único `&`**. Os dois operandos de `&` e `||` são avaliados, sem curto-circuito.

As declarações ficam em uma seção `[...]` antes do corpo `{...}`. As funções são agrupadas em uma única seção `funcao [...]`, entre as declarações globais e o programa principal. Um bloco precisa conter ao menos um comando; `{ ; }` representa um bloco sem operação.

## Estrutura do projeto

| Arquivo | Responsabilidade |
| --- | --- |
| `g-v2.l` | Regras léxicas e reconhecimento de tokens |
| `g-v2.y` | Gramática, construção da AST e função `main` |
| `ast.c` / `ast.h` | Criação, impressão e liberação dos nós da AST |
| `symtab.c` / `symtab.h` | Tabela de símbolos, funções, parâmetros e escopos |
| `semantic.c` / `semantic.h` | Verificações semânticas e de tipos |
| `codagen.c` / `codagen.h` | Geração de Assembly MIPS |
| `Makefile` | Construção e limpeza do compilador |
| `examples/` | Programas de exemplo em G-V2 |

Os arquivos `lex.yy.c`, `g-v2.tab.c` e `g-v2.tab.h` são gerados durante a construção e não precisam ser versionados.

## Limitações e validação

- Não há tipos de ponto flutuante nem um tipo de variável para strings; os literais de texto são aceitos em `escreva`.
- Comentários de linha com `//` não fazem parte das regras léxicas.
- Não há verificação dos limites de índices de vetores no código gerado.
- Não há uma suíte automatizada de testes neste projeto.
- O README e os exemplos foram preparados a partir da leitura do código. A compilação e a execução dos exemplos ainda precisam ser verificadas em um ambiente com as dependências e um simulador MIPS.

## Licença

Este repositório ainda não define uma licença de uso ou redistribuição.
