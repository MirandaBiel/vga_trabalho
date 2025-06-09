#include <stdio.h>

#define IO_JTAG_UART (*(volatile unsigned int *)0xFF201000)

// Lê um caractere da JTAG_UART com polling
char read_char() {
    unsigned int data;
    do {
        data = IO_JTAG_UART;
    } while ((data & 0x8000) == 0); // Espera até bit 15 estar setado (dado valido)

    char c = data & 0xFF;

    // Apenas caracteres ASCII imprimiveis
    if (c >= 32 && c <= 126) {
        putchar(c);        // Ecoa o caractere
        fflush(stdout);    // Forca exibir imediatamente
    } else if (c == '\r' || c == '\n') {
        putchar('\n');
    } else if (c == 8 || c == 127) {
        printf("\b \b");   // Backspace
    }

    return c;
}

// Lê uma linha com eco e backspace simples
void read_line(char *buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = read_char();

        if (c == '\r' || c == '\n') {
            break;
        } else if ((c == 8 || c == 127) && i > 0) {
            i--;
        } else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
}

int main() {
    char input[100];

    printf("Digite algo e pressione Enter (somente letras simples):\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        read_line(input, 100);
        printf("Voce digitou: %s\n", input);
    }

    return 0;
}
