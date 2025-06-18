#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>

// =================================================================================
// --- CONFIGURAÇÕES DE HARDWARE E TELA ---
// =================================================================================
#define PERIPHERAL_BASE 0xFF200000
#define PERIPHERAL_SIZE 0x00001000 
#define DEVICES_BUTTONS 0x0050

#define FRAME_BASE      0xC8000000
#define LWIDTH          512
#define VISIBLE_WIDTH   320
#define VISIBLE_HEIGHT  240
#define PIXEL_SIZE      2

// =================================================================================
// --- CONFIGURAÇÕES DO JOGO ---
// =================================================================================
#define BIRD_X_POS       60
#define BIRD_RADIUS      10
#define GRAVITY          0.4
#define JUMP_VELOCITY    -6.0 // <-- MUDANÇA 1: Pulo mais curto
#define OBSTACLE_SPEED   3
#define OBSTACLE_WIDTH   50
#define GAP_HEIGHT       80
#define OBSTACLE_SPACING 200

// =================================================================================
// --- CORES ---
// =================================================================================
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0 
#define YELLOW  0xFFE0 
#define SKY_BLUE 0x841F
#define GRAY    0x8410

// =================================================================================
// --- MUDANÇA 2: SEÇÃO DA FONTE ADICIONADA ---
// =================================================================================
#define FONT_WIDTH 3
#define FONT_HEIGHT 5
#define FONT_CHAR_SPACING 2 
#define FONT_SCALE 2        

const int font_3x5[10][FONT_HEIGHT][FONT_WIDTH] = {
    {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}}, // 0
    {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}}, // 1
    {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}}, // 2
    {{1,1,1},{0,0,1},{0,1,1},{0,0,1},{1,1,1}}, // 3
    {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}}, // 4
    {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}}, // 5
    {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}}, // 6
    {{1,1,1},{0,0,1},{0,1,0},{0,1,0},{0,1,0}}, // 7
    {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}}, // 8
    {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}  // 9
};

// =================================================================================
// --- VARIÁVEIS GLOBAIS DE HARDWARE ---
// =================================================================================
int mem_fd = -1; // Usaremos um único descritor de arquivo
volatile uint16_t (*tela)[LWIDTH];
volatile void *peripheral_map = NULL;
volatile unsigned int *key_ptr = NULL;

// =================================================================================
// --- ESTRUTURAS E ESTADOS DE JOGO ---
// =================================================================================
typedef enum {
    GAME_RUNNING,
    GAME_OVER
} GameState;

typedef struct { double y, velocity_y; } Bird;
typedef struct { int x, gap_y, scored; } Obstacle;

// --- PROTÓTIPOS ---
void reset_game(Bird* bird, Obstacle obstacles[], int* score);
void draw_score(int score, int x, int y, uint16_t color);

// =================================================================================
// --- FUNÇÕES DE INICIALIZAÇÃO E LIMPEZA DE HARDWARE ---
// =================================================================================
void cleanup_resources() {
    if (tela) munmap((void*)tela, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE);
    if (peripheral_map) munmap((void*)peripheral_map, PERIPHERAL_SIZE);
    if (mem_fd != -1) close(mem_fd);
    printf("\nRecursos liberados. Saindo do jogo.\n");
}

int init_hardware() {
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd == -1) { perror("Erro ao abrir /dev/mem"); return -1; }
    
    void *framebuffer_map = mmap(NULL, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, FRAME_BASE);
    if (framebuffer_map == MAP_FAILED) { perror("Erro ao mapear VGA"); close(mem_fd); return -1; }
    tela = (volatile uint16_t (*)[LWIDTH]) framebuffer_map;

    peripheral_map = mmap(NULL, PERIPHERAL_SIZE, PROT_READ, MAP_SHARED, mem_fd, PERIPHERAL_BASE);
    if (peripheral_map == MAP_FAILED) { perror("Erro ao mapear periféricos"); munmap(framebuffer_map, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE); close(mem_fd); return -1; }
    
    key_ptr = (volatile unsigned int *)(peripheral_map + DEVICES_BUTTONS);

    atexit(cleanup_resources);
    return 0;
}

// =================================================================================
// --- FUNÇÕES DE DESENHO ---
// =================================================================================
void set_pix(int x, int y, uint16_t color) {
    if (y >= 0 && y < VISIBLE_HEIGHT && x >= 0 && x < VISIBLE_WIDTH) {
        tela[y][x] = color;
    }
}

void draw_filled_rect(int x0, int y0, int x1, int y1, uint16_t color) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            set_pix(x, y, color);
        }
    }
}

void draw_circle(int xc, int yc, int r, uint16_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                set_pix(xc + x, yc + y, color);
            }
        }
    }
}

void fill_screen(uint16_t color) {
    for (int y = 0; y < VISIBLE_HEIGHT; y++) {
        for (int x = 0; x < VISIBLE_WIDTH; x++) {
            tela[y][x] = color;
        }
    }
}

// --- MUDANÇA 2: FUNÇÕES DE DESENHO DO PLACAR ADICIONADAS ---
void draw_digit(int digit, int x, int y, uint16_t color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < FONT_HEIGHT; row++) {
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (font_3x5[digit][row][col] == 1) {
                draw_filled_rect(x + (col * FONT_SCALE), y + (row * FONT_SCALE),
                                 x + (col * FONT_SCALE) + FONT_SCALE, y + (row * FONT_SCALE) + FONT_SCALE,
                                 color);
            }
        }
    }
}

void draw_score(int score, int x, int y, uint16_t color) {
    char score_text[10];
    sprintf(score_text, "%d", score);
    int len = strlen(score_text);
    int current_x = x;

    for (int i = len - 1; i >= 0; i--) {
        int digit = score_text[i] - '0';
        int char_width = (FONT_WIDTH * FONT_SCALE);
        current_x -= char_width;
        draw_digit(digit, current_x, y, color);
        current_x -= FONT_CHAR_SPACING;
    }
}

// =================================================================================
// --- LÓGICA DO JOGO ---
// =================================================================================
int check_collision(const Bird* bird, const Obstacle* obs) {
    if ((bird->y - BIRD_RADIUS) < 0 || (bird->y + BIRD_RADIUS) > VISIBLE_HEIGHT) {
        return 1;
    }
    if (BIRD_X_POS + BIRD_RADIUS > obs->x && BIRD_X_POS - BIRD_RADIUS < obs->x + OBSTACLE_WIDTH) {
        if (bird->y - BIRD_RADIUS < obs->gap_y || bird->y + BIRD_RADIUS > obs->gap_y + GAP_HEIGHT) {
            return 1;
        }
    }
    return 0;
}

void reset_game(Bird* bird, Obstacle obstacles[], int* score) {
    bird->y = VISIBLE_HEIGHT / 2.0;
    bird->velocity_y = 0;
    *score = 0;

    for (int i = 0; i < 2; i++) {
        obstacles[i].x = VISIBLE_WIDTH + 100 + i * OBSTACLE_SPACING;
        obstacles[i].gap_y = rand() % (VISIBLE_HEIGHT - GAP_HEIGHT - 60) + 30;
        obstacles[i].scored = 0;
    }
    printf("Iniciando Jogo! Pontuacao: 0\n");
}


int main() {
    if (init_hardware() != 0) { return 1; }

    srand(time(NULL));

    Bird bird;
    Obstacle obstacles[2];
    int score;
    GameState state = GAME_RUNNING;
    
    unsigned int prev_key_state = 0x0;

    reset_game(&bird, obstacles, &score);

    while (1) {
        unsigned int current_key_state = *key_ptr;
        
        if (current_key_state & 0b0001) {
            break; 
        }

        switch(state) {
            case GAME_RUNNING: {
                int key1_pressed_now = (current_key_state & 0b0010);
                int key1_pressed_before = (prev_key_state & 0b0010);
                if (key1_pressed_now && !key1_pressed_before) {
                    bird.velocity_y = JUMP_VELOCITY;
                }

                bird.velocity_y += GRAVITY;
                bird.y += bird.velocity_y;

                for (int i = 0; i < 2; i++) {
                    obstacles[i].x -= OBSTACLE_SPEED;

                    if (!obstacles[i].scored && obstacles[i].x + OBSTACLE_WIDTH < BIRD_X_POS) {
                        obstacles[i].scored = 1;
                        score++;
                        printf("Pontuacao: %d\n", score);
                    }

                    if (obstacles[i].x + OBSTACLE_WIDTH < 0) {
                        obstacles[i].x = VISIBLE_WIDTH;
                        obstacles[i].gap_y = rand() % (VISIBLE_HEIGHT - GAP_HEIGHT - 60) + 30;
                        obstacles[i].scored = 0;
                    }
                }

                for (int i = 0; i < 2; i++) {
                    if (check_collision(&bird, &obstacles[i])) {
                        state = GAME_OVER;
                        printf("====================\n");
                        printf("    FIM DE JOGO\n");
                        printf("  Pontuacao Final: %d\n", score);
                        printf("====================\n");
                        printf("Pressione KEY1 para reiniciar ou KEY0 para sair.\n");
                    }
                }

                fill_screen(SKY_BLUE);
                for (int i = 0; i < 2; i++) {
                    draw_filled_rect(obstacles[i].x, 0, obstacles[i].x + OBSTACLE_WIDTH, obstacles[i].gap_y, GREEN);
                    draw_filled_rect(obstacles[i].x, obstacles[i].gap_y + GAP_HEIGHT, obstacles[i].x + OBSTACLE_WIDTH, VISIBLE_HEIGHT, GREEN);
                }
                draw_circle(BIRD_X_POS, (int)bird.y, BIRD_RADIUS, YELLOW);
                
                // --- MUDANÇA 2: CHAMADA PARA DESENHAR O PLACAR ---
                draw_score(score, VISIBLE_WIDTH - 10, 10, WHITE);

                break;
            } 

            case GAME_OVER: {
                draw_circle(BIRD_X_POS, (int)bird.y, BIRD_RADIUS, RED);

                int key1_pressed_now = (current_key_state & 0b0010);
                int key1_pressed_before = (prev_key_state & 0b0010);
                if (key1_pressed_now && !key1_pressed_before) {
                    reset_game(&bird, obstacles, &score);
                    state = GAME_RUNNING;
                }
                break;
            }
        } 

        prev_key_state = current_key_state;
        usleep(16666); // ~60 FPS
    }
    
    return 0;
}