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
