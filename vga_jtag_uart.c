#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>  // Para uint16_t

// Definições da interface JTAG_UART (entrada/saída via terminal)
#define IO_JTAG_UART (*(volatile unsigned int *)0xFF201000)

// Base da memória VGA
#define VGA_BASE 0xC8000000
#define SCREEN_WIDTH 512         // Largura total da tela
#define VISIBLE_WIDTH 320        // Largura visível
#define VISIBLE_HEIGHT 240       // Altura visível

// Definições de cores (formato RGB 5-6-5)
#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define GRAY    0x8410
#define WHITE   0xFFFF
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define ORANGE  0xFC00
#define PURPLE  0x780F
#define BROWN   0xA145
#define PINK    0xF81F
#define LIME    0x07F0
#define NAVY    0x000F
#define TEAL    0x0410

// Ponteiro para a matriz de pixels na memória VGA
uint16_t *ppix = (uint16_t *)VGA_BASE;

// Cor atual para desenhar
uint16_t current_color = WHITE;

// Função para ler um caractere da UART (com polling)
char read_char() {
    unsigned int data;
    do {
        data = IO_JTAG_UART;
    } while ((data & 0x8000) == 0); // Espera até que bit 15 indique dado disponível
    return data & 0xFF; // Retorna os bits 7:0 (caractere)
}

// Função para ler uma linha de texto do terminal (com eco e suporte a backspace)
void read_line(char *buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = read_char();
        if (c == '\r' || c == '\n') break;  // Enter finaliza
        if ((c == 8 || c == 127)) {         // Backspace ou DEL
            if (i > 0) {
                i--;
                printf("\b \b");            // Apaga no terminal
                fflush(stdout);
            }
        } else if (c >= 32 && c <= 126) {   // Caracteres imprimíveis
            buffer[i++] = c;
            putchar(c);                    // Eco na tela
            fflush(stdout);
        }
    }
    buffer[i] = '\0';
    putchar('\n');
}

// Converte uma string para letras maiúsculas
void to_upper(char *str) {
    while (*str) {
        *str = toupper(*str);
        str++;
    }
}

// Define um pixel na tela com a cor atual
void set_pix(int lin, int col) {
    if (lin < 0 || lin >= VISIBLE_HEIGHT || col < 0 || col >= VISIBLE_WIDTH) return;
    ppix[lin * SCREEN_WIDTH + col] = current_color;
}

// Algoritmo de Bresenham para desenhar uma linha
void draw_line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        set_pix(y0, x0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Algoritmo de Bresenham para desenhar um círculo
void draw_circle(int lin, int col, int r) {
    int x = -r, y = 0, err = 2 - 2 * r;
    do {
        set_pix(lin - y, col + x);
        set_pix(lin - x, col - y);
        set_pix(lin + y, col - x);
        set_pix(lin + x, col + y);
        int e2 = err;
        if (e2 <= y) err += ++y * 2 + 1;
        if (e2 > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
}

// Desenha as bordas de um retângulo (vazado)
void draw_rect(int y0, int x0, int y1, int x1) {
    draw_line(x0, y0, x1, y0);
    draw_line(x1, y0, x1, y1);
    draw_line(x1, y1, x0, y1);
    draw_line(x0, y1, x0, y0);
}

// Desenha um retângulo preenchido (tile)
void draw_tile(int y0, int x0, int y1, int x1) {
    int ymin = y0 < y1 ? y0 : y1;
    int ymax = y0 > y1 ? y0 : y1;
    int xmin = x0 < x1 ? x0 : x1;
    int xmax = x0 > x1 ? x0 : x1;

    for (int y = ymin; y <= ymax; y++) {
        for (int x = xmin; x <= xmax; x++) {
            set_pix(y, x);
        }
    }
}

// Preenche toda a tela com a cor atual
void fill_screen() {
    for (int y = 0; y < VISIBLE_HEIGHT; y++) {
        for (int x = 0; x < VISIBLE_WIDTH; x++) {
            set_pix(y, x);
        }
    }
}

// Mostra o menu de opções para o usuário
void print_menu() {
    printf("\nMenu de opcoes:\n");
    printf("1) COLOR - Define a cor atual\n");
    printf("2) LINE  - Desenha uma linha\n");
    printf("3) CIRC  - Desenha um circulo\n");
    printf("4) RECT  - Desenha um retangulo vazado\n");
    printf("5) TILE  - Desenha um retangulo cheio\n");
    printf("6) FUNDO - Preenche a tela\n");
}

// Define a cor atual com base no nome digitado
void set_color(char *color_name) {
    if (strcmp(color_name, "BLACK") == 0) current_color = BLACK;
    else if (strcmp(color_name, "RED") == 0) current_color = RED;
    else if (strcmp(color_name, "GREEN") == 0) current_color = GREEN;
    else if (strcmp(color_name, "BLUE") == 0) current_color = BLUE;
    else if (strcmp(color_name, "GRAY") == 0) current_color = GRAY;
    else if (strcmp(color_name, "WHITE") == 0) current_color = WHITE;
    else if (strcmp(color_name, "YELLOW") == 0) current_color = YELLOW;
    else if (strcmp(color_name, "CYAN") == 0) current_color = CYAN;
    else if (strcmp(color_name, "MAGENTA") == 0) current_color = MAGENTA;
    else if (strcmp(color_name, "ORANGE") == 0) current_color = ORANGE;
    else if (strcmp(color_name, "PURPLE") == 0) current_color = PURPLE;
    else if (strcmp(color_name, "BROWN") == 0) current_color = BROWN;
    else if (strcmp(color_name, "PINK") == 0) current_color = PINK;
    else if (strcmp(color_name, "LIME") == 0) current_color = LIME;
    else if (strcmp(color_name, "NAVY") == 0) current_color = NAVY;
    else if (strcmp(color_name, "TEAL") == 0) current_color = TEAL;
    else {
        printf("Entrada invalida\n");
        return;
    }
    printf("Cor definida como %s\n", color_name);
}


// Função principal
int main() {
    char input[100];

    printf("Sistema de desenho VGA - DE1-SoC\n");

    while (1) {
        print_menu();
        printf("> ");
        read_line(input, 100);
        to_upper(input);

        // Opção COLOR
        if (strcmp(input, "1") == 0 || strcmp(input, "COLOR") == 0) {
            printf("Formato (COLOR): <cor>\n");
            printf("Cores: BLACK RED GREEN BLUE GRAY WHITE ");
            printf("YELLOW CYAN MAGENTA ORANGE PURPLE BROWN PINK LIME NAVY TEAL\n> ");
            read_line(input, 100);
            to_upper(input);
            set_color(input);
        }

        // Opção LINE
        else if (strcmp(input, "2") == 0 || strcmp(input, "LINE") == 0) {
            int y0, x0, y1, x1;
            printf("Formato (LINE): lin0 col0 lin1 col1\n> ");
            read_line(input, 100);
            if (sscanf(input, "%d %d %d %d", &y0, &x0, &y1, &x1) == 4) {
                draw_line(x0, y0, x1, y1);
            } else {
                printf("Entrada invalida\n");
            }
        }

        // Opção CIRC
        else if (strcmp(input, "3") == 0 || strcmp(input, "CIRC") == 0) {
            int lin, col, r;
            printf("Formato (CIRC): lin col raio\n> ");
            read_line(input, 100);
            if (sscanf(input, "%d %d %d", &lin, &col, &r) == 3) {
                draw_circle(lin, col, r);
            } else {
                printf("Entrada invalida\n");
            }
        }

        // Opção RECT
        else if (strcmp(input, "4") == 0 || strcmp(input, "RECT") == 0) {
            int y0, x0, y1, x1;
            printf("Formato (RECT): lin0 col0 lin1 col1\n> ");
            read_line(input, 100);
            if (sscanf(input, "%d %d %d %d", &y0, &x0, &y1, &x1) == 4) {
                draw_rect(y0, x0, y1, x1);
            } else {
                printf("Entrada invalida\n");
            }
        }

        // Opção TILE
        else if (strcmp(input, "5") == 0 || strcmp(input, "TILE") == 0) {
            int y0, x0, y1, x1;
            printf("Formato (TILE): lin0 col0 lin1 col1\n> ");
            read_line(input, 100);
            if (sscanf(input, "%d %d %d %d", &y0, &x0, &y1, &x1) == 4) {
                draw_tile(y0, x0, y1, x1);
            } else {
                printf("Entrada invalida\n");
            }
        }

        // Opção FUNDO
        else if (strcmp(input, "6") == 0 || strcmp(input, "FUNDO") == 0) {
            fill_screen();
            printf("Tela preenchida com a cor atual\n");
        }

        // Entrada inválida
        else {
            printf("Entrada invalida\n");
        }
    }

    return 0;
}
