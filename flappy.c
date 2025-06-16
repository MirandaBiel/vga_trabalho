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
#define JUMP_VELOCITY    -7.0
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
// --- VARIÁVEIS GLOBAIS DE HARDWARE ---
// =================================================================================
int mem_fd_vga = -1, mem_fd_peripherals = -1;
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

typedef struct {
    double y;
    double velocity_y;
} Bird;

typedef struct {
    int x;
    int gap_y;
    int scored;
} Obstacle;

// --- PROTÓTIPOS ---
void reset_game(Bird* bird, Obstacle obstacles[], int* score);

// =================================================================================
// --- FUNÇÕES DE INICIALIZAÇÃO E LIMPEZA DE HARDWARE ---
// =================================================================================
void cleanup_resources() {
    if (tela) munmap((void*)tela, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE);
    if (peripheral_map) munmap((void*)peripheral_map, PERIPHERAL_SIZE);
    if (mem_fd_vga != -1) close(mem_fd_vga);
    if (mem_fd_peripherals != -1) close(mem_fd_peripherals);
    printf("\nRecursos liberados. Saindo do jogo.\n");
}

int init_hardware() {
    mem_fd_vga = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd_vga == -1) { perror("Erro ao abrir /dev/mem para VGA"); return -1; }
    
    void *framebuffer_map = mmap(NULL, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd_vga, FRAME_BASE);
    if (framebuffer_map == MAP_FAILED) { perror("Erro ao mapear VGA"); close(mem_fd_vga); return -1; }
    tela = (volatile uint16_t (*)[LWIDTH]) framebuffer_map;

    mem_fd_peripherals = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd_peripherals == -1) { perror("Erro ao abrir /dev/mem para Periféricos"); munmap(framebuffer_map, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE); close(mem_fd_vga); return -1; }
    
    peripheral_map = mmap(NULL, PERIPHERAL_SIZE, PROT_READ, MAP_SHARED, mem_fd_peripherals, PERIPHERAL_BASE);
    if (peripheral_map == MAP_FAILED) { perror("Erro ao mapear periféricos"); munmap(framebuffer_map, LWIDTH * VISIBLE_HEIGHT * PIXEL_SIZE); close(mem_fd_vga); close(mem_fd_peripherals); return -1; }
    
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
    
    unsigned int prev_key_state = 0x0; // Estado inicial dos botões (todos soltos = 0)

    reset_game(&bird, obstacles, &score);

    while (1) { // Loop principal infinito, controlado por KEY0
        unsigned int current_key_state = *key_ptr;
        
        // --- Verificação de Saída (KEY0) --- Sempre ativa
        if (current_key_state & 0b0001) { // KEY0 pressionado (ativo ALTO)
            break; // Sai do loop principal e encerra o programa
        }

        // --- Máquina de Estados do Jogo ---
        switch(state) {
            case GAME_RUNNING: {
                // --- 1. LIDAR COM ENTRADA (INPUT) ---
                int key1_pressed_now = (current_key_state & 0b0010);
                int key1_pressed_before = (prev_key_state & 0b0010);
                if (key1_pressed_now && !key1_pressed_before) {
                    bird.velocity_y = JUMP_VELOCITY;
                }

                // --- 2. ATUALIZAR ESTADO DO JOGO ---
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

                // --- 3. VERIFICAR COLISÕES ---
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

                // --- 4. DESENHAR TUDO ---
                fill_screen(SKY_BLUE);
                for (int i = 0; i < 2; i++) {
                    draw_filled_rect(obstacles[i].x, 0, obstacles[i].x + OBSTACLE_WIDTH, obstacles[i].gap_y, GREEN);
                    draw_filled_rect(obstacles[i].x, obstacles[i].gap_y + GAP_HEIGHT, obstacles[i].x + OBSTACLE_WIDTH, VISIBLE_HEIGHT, GREEN);
                }
                draw_circle(BIRD_X_POS, (int)bird.y, BIRD_RADIUS, YELLOW);
                break;
            } // Fim do case GAME_RUNNING

            case GAME_OVER: {
                // Na tela de Game Over, pisca uma mensagem
                draw_filled_rect(50, 100, VISIBLE_WIDTH - 50, 140, GRAY); // Fundo para o texto
                
                // Pisca o pássaro em vermelho
                draw_circle(BIRD_X_POS, (int)bird.y, BIRD_RADIUS, RED);

                int key1_pressed_now = (current_key_state & 0b0010);
                int key1_pressed_before = (prev_key_state & 0b0010);
                if (key1_pressed_now && !key1_pressed_before) {
                    reset_game(&bird, obstacles, &score);
                    state = GAME_RUNNING;
                }
                break;
            } // Fim do case GAME_OVER
        } // Fim do switch

        prev_key_state = current_key_state;
        usleep(16666); // ~60 FPS
    }
    
    return 0; // atexit() cuidará da limpeza
}