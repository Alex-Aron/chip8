#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
//#include <SDL2/SDL.h>

#define INST_COUNT (34)
#define FONT_START (0x50)
#define NNN(i) (i & 0x0FFF)
#define NIBBLE(i) ((i) & 0xF)
#define NIBBLE2(i) ((i >> 4) & 0xF)
#define NIBBLE3(i) ((i >> 8) & 0xF)
#define NIBBLE4(i) ((i >> 12) & 0xF)
#define VX(i) ((i >> 8) & 0x0F)
#define VY(i) ((i >> 4) & 0x0F)
#define KK(i) (i & 0x0FF)
#define VF (15)
#define PC_START 0x200

uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

typedef struct {
    uint8_t memory[4096]; //mem
    uint8_t V[16]; //registers
    uint16_t I; //index reg
    uint16_t pc; //program coutner
    uint8_t sp; //stack pointer
    uint16_t stack[16];
    uint8_t display[64 * 32];
    bool keys[16];
    uint8_t delay_timer;
    uint8_t sound_timer;
} chip8_t;

static inline void clr(chip8_t* chip8, uint16_t opcode) { memset(chip8->display, 0, sizeof(chip8->display)); }

static inline void ret(chip8_t* chip8, uint16_t opcode) { chip8->pc = chip8->stack[chip8->sp--] ; }

static inline void jpaddr(chip8_t* chip8, uint16_t opcode) { chip8->pc = NNN(opcode) ; }

static inline void call(chip8_t* chip8, uint16_t opcode) {
    chip8->sp++;
    chip8->stack[chip8->sp] = chip8->pc;
    chip8->pc = NNN(opcode);
} 
static inline void se(chip8_t* chip8, uint16_t opcode) { chip8->pc = (chip8->V[VX(opcode)] == KK(opcode)) ? chip8->pc + 2 : chip8->pc; }

static inline void sne(chip8_t* chip8, uint16_t opcode) { chip8->pc = (chip8->V[VX(opcode)] != KK(opcode)) ? chip8->pc + 2 : chip8->pc; }

static inline void sre(chip8_t* chip8, uint16_t opcode) { chip8->pc = (chip8->V[VX(opcode)] == chip8->V[VY(opcode)]) ? chip8->pc + 2 : chip8->pc; }

static inline void load(chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] = KK(opcode); }

static inline void add(chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] += KK(opcode); }

static inline void loadr(chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] = chip8->V[VY(opcode)]; }

static inline void or (chip8_t * chip8, uint16_t opcode) { chip8->V[VX(opcode)] = chip8->V[VY(opcode)] | chip8->V[VX(opcode)]; }

static inline void and (chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] = chip8->V[VY(opcode)] & chip8->V[VX(opcode)]; }

static inline void xor (chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] = chip8->V[VY(opcode)] ^ chip8->V[VX(opcode)]; }

static inline void addxy(chip8_t* chip8, uint16_t opcode) {
    uint8_t vx_val = chip8->V[VX(opcode)]; // Store original Vx
    uint8_t vy_val = chip8->V[VY(opcode)];
    uint16_t sum = (uint16_t)vx_val + vy_val;
    chip8->V[VX(opcode)] = sum & 0xFF; // Store the truncated sum
    chip8->V[VF] = (sum > 255) ? 1 : 0;
}

static inline void sub(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VF] = (chip8->V[VX(opcode)] > chip8->V[VY(opcode)]) ? 1 : 0;
    chip8->V[VX(opcode)] -= chip8->V[VY(opcode)];
}
static inline void shr(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VF] = chip8->V[VX(opcode)] & 0x01; //if lsb is one vf will be one
    chip8->V[VX(opcode)] = chip8->V[VX(opcode)] >> 1;

}
static inline void subn(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VF] = (chip8->V[VY(opcode)] > chip8->V[VX(opcode)]) ? 1 : 0;
    chip8->V[VX(opcode)] = chip8->V[VY(opcode)] - chip8->V[VX(opcode)];
}

static inline void shl(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VF] = (chip8->V[VX(opcode)] & 0x80) ? 1 : 0; // Correctly get MSB
    chip8->V[VX(opcode)] = chip8->V[VX(opcode)] << 1;
}

static inline void loadI(chip8_t* chip8, uint16_t opcode) {
    chip8->I = NNN(opcode);
}

static inline void jmp(chip8_t* chip8, uint16_t opcode) {
    chip8->pc = chip8->V[0] + NNN(opcode);
}

static inline void rnd(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VX(opcode)] = (rand() % 256) & KK(opcode);
}

static inline void draw(chip8_t* chip8, uint16_t opcode) {
    uint8_t x_coord_start = chip8->V[VX(opcode)] % 64;
    uint8_t y_coord_start = chip8->V[VY(opcode)] % 32;
    uint8_t height = NIBBLE(opcode); // N: height of the sprite

    chip8->V[VF] = 0; // Reset collision flag

    for (int row = 0; row < height; row++) {
        uint8_t sprite_byte = chip8->memory[chip8->I + row];
        uint8_t current_y = y_coord_start + row;

        // Optional: Clipping for y-coordinate (can also be handled by modulo)
        // if (current_y >= 32) continue; // or current_y %= 32;

        for (int bit_pos = 0; bit_pos < 8; bit_pos++) { // Sprites are 8 pixels wide
            uint8_t current_x = x_coord_start + bit_pos;

            // Optional: Clipping for x-coordinate (can also be handled by modulo)
            // if (current_x >= 64) continue; // or current_x %= 64;

            // Check if the current bit in the sprite_byte is set
            if ((sprite_byte & (0x80 >> bit_pos)) != 0) {
                // Wrap around if coordinates go off screen
                uint16_t display_index = ((current_y % 32) * 64) + (current_x % 64);

                if (chip8->display[display_index] == 1) { // Check if pixel on screen is already set
                    chip8->V[VF] = 1; // Collision detected
                }
                chip8->display[display_index] ^= 1; // XOR the pixel
            }
        }
    }
}

static inline void skip(chip8_t* chip8, uint16_t opcode) { if (chip8->keys[chip8->V[VX(opcode)]]) chip8->pc += 2; }

static inline void skip_not(chip8_t* chip8, uint16_t opcode) { if (!(chip8->keys[chip8->V[VX(opcode)]])) chip8->pc += 2; }

static inline void loadDT(chip8_t* chip8, uint16_t opcode) { chip8->V[VX(opcode)] = chip8->delay_timer; }

static inline void loadKey(chip8_t* chip8, uint16_t opcode) {
    bool key_found = false;
    uint8_t key = 0;
    for (int i = 0; i < 16; i++) {
        if (chip8->keys[i]) {
            key_found = true;
            key = i;
            break;
        }
    }
    if (!key_found) {
        chip8->pc -= 2; //if no key is pressed, we don't increment the pc
        return;
    }
    chip8->V[VX(opcode)] = key;
}

static inline void setDT(chip8_t* chip8, uint16_t opcode) { chip8->delay_timer = chip8->V[VX(opcode)]; }

static inline void setST(chip8_t* chip8, uint16_t opcode) { chip8->sound_timer = chip8->V[VX(opcode)]; }

static inline void addI(chip8_t* chip8, uint16_t opcode) { chip8->I += chip8->V[VX(opcode)]; }

static inline void loadF(chip8_t* chip8, uint16_t opcode) { chip8->I = chip8->V[VX(opcode)] * 5 + FONT_START; }

static inline void loadB(chip8_t* chip8, uint16_t opcode) {
    uint8_t val = chip8->V[VX(opcode)];
    chip8->memory[chip8->I + 2] = val % 10; //lsb 
    val /= 10;
    chip8->memory[chip8->I + 1] = val % 10; //lsb 
    val /= 10;
    chip8->memory[chip8->I] = val % 10;
}

static inline void readRegs(chip8_t* chip8, uint16_t opcode) {
    for (int i = 0; i <= VX(opcode); i++)
        chip8->memory[chip8->I + i] = chip8->V[i];
}

static inline void writeRegs(chip8_t* chip8, uint16_t opcode) {
    for (int i = 0; i <= VX(opcode); i++)
        chip8->V[i] = chip8->memory[chip8->I + i];
}

typedef void (*instruction_func)(chip8_t* chip8, uint16_t opcode); //pointer to a instruction function
instruction_func instructions[INST_COUNT] = {
    clr, //0
    ret, //1
    jpaddr, // .2
    call, // 3
    se, // 4
    sne, // 5
    sre, // 6
    load, //7
    add, //8
    loadr, //9
    or , //10
    and, //11
    xor, //12
    addxy, //13
    sub, //14
    shr, //15
    subn, //16
    shl, //17
    sne, //18
    loadI, // 19
    jmp, //20
    rnd, //21 
    draw, //22
    skip,//23
    skip_not,//24
    loadDT,//25
    loadKey,//26
    setDT,//27
    setST,//28
    addI,//29
    loadF,//30
    loadB,//31
    readRegs,//32
    writeRegs //33
};

// ### definitely could have went a better way for decoding
//Already wrote all the functions and I just need the index for the funcptr, the opcode
//is used properly in each function
static inline int decode(uint16_t opcode) {
    printf("Nibble 4 is: %d\n", NIBBLE4(opcode));
    printf("Nibble 3 is: %d\n", NIBBLE3(opcode));
    printf("Nibble 2 is: %d\n", NIBBLE2(opcode));
    printf("Nibble is: %d\n", NIBBLE(opcode));
    printf("VX is: %d\n", VX(opcode));
    switch (NIBBLE4(opcode)) {
    case 0:
        return (NIBBLE(opcode) == 0) ? 0 : 1;
        break;
    case 1:
        return 2;
        break;
    case 2:
        return 3;
        break;
    case 3:
        return 4;
        break;
    case 4:
        return 5;
        break;
    case 5:
        return 6;
        break;
    case 6:
        return 7;
        break;
    case 7:
        return 8;
        break;
    case 8:
        switch (NIBBLE(opcode)) {
        case 0:
            return 9;
            break;
        case 1:
            return 10;
            break;
        case 2:
            return 11;
            break;
        case 3:
            return 12;
            break;
        case 4:
            return 13;
            break;
        case 5:
            return 14;
            break;
        case 6:
            return 15;
            break;
        case 7:
            return 16;
            break;
        case 0xE:
            return 17;
            break;
        default:
            printf("Incorrect instruction");
            return -1;
        }
        break;
    case 9:
        return 18;
        break;
    case 0xA:
        return 19;
        break;
    case 0xB:
        return 20;
        break;
    case 0xC:
        return 21;
        break;
    case 0xD:
        return 22;
        break;
    case 0xE:
        return (NIBBLE(opcode) == 0xE) ? 23 : 24; //assumes only 1 or E will be passed
        break;
    case 0xF:
        switch (opcode & 0x0FF) {
        case 0x07:
            return 25;
            break;
        case 0x0A:
            return 26;
            break;
        case 0x15:
            return 27;
            break;
        case 0x18:
            return 28;
            break;
        case 0x1E:
            return 29;
            break;
        case 0x29:
            return 30;
            break;
        case 0x33:
            return 31;
            break;
        case 0x55:
            return 32;
            break;
        case 0x65:
            return 33;
            break;
        default:
            printf("Incorrect instruction");
            return -1;
        }
        break;
    default:
        printf("Incorrect instruction");
        return -1;

    }
}

void cycle(chip8_t* chip8, uint16_t opcode) {
    //fetch (get the opcode)
    //execute the instruction
    //make any additional changes as needy
    printf("Current PC: 0x%04X\n", chip8->pc);
    uint8_t inst_index = decode(opcode);
    chip8->pc += 2;
    printf("Executing instruction %d with opcode 0x%04X\n", inst_index, opcode);
    fflush(stdout);
    instructions[inst_index](chip8, opcode); //execute step
    //if (chip8->delay_timer > 0) chip8->delay_timer--;
    //if (chip8->sound_timer > 0) chip8->sound_timer--;
}

void init(chip8_t* chip8, FILE* rom) {
    //clear all arrays and also load font into memory. 
    chip8->I = 0;
    chip8->sp = 0;
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    memset(chip8->memory, 0, 4096);
    memset(chip8->V, 0, 16);
    memset(chip8->display, 0, 2048);
    memset(chip8->keys, 0, 16);
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memcpy(chip8->memory, fontset, sizeof(fontset));
    fread(chip8->memory + PC_START, 1, 4096 - PC_START, rom);
    fclose(rom);
    chip8->pc = PC_START; //set the program counter to the start of the program
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Should input ./chip8 <rom>\n");
        return 0;
    }
    printf("Loading ROM: %s\n", argv[1]);
    FILE* rom = fopen(argv[1], "rb");
    if (!rom) {
        printf("Failed to open ROM file: %s\n", argv[1]);
        return 1;
    }

    chip8_t chip8;
    init(&chip8, rom);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        64 * 10, 32 * 10,
        SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event e;
    uint32_t last_timer_tick = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool pressed = e.type == SDL_KEYDOWN;
                switch (e.key.keysym.sym) {
                case SDLK_1: chip8.keys[0x1] = pressed; break;
                case SDLK_2: chip8.keys[0x2] = pressed; break;
                case SDLK_3: chip8.keys[0x3] = pressed; break;
                case SDLK_4: chip8.keys[0xC] = pressed; break;
                case SDLK_q: chip8.keys[0x4] = pressed; break;
                case SDLK_w: chip8.keys[0x5] = pressed; break;
                case SDLK_e: chip8.keys[0x6] = pressed; break;
                case SDLK_r: chip8.keys[0xD] = pressed; break;
                case SDLK_a: chip8.keys[0x7] = pressed; break;
                case SDLK_s: chip8.keys[0x8] = pressed; break;
                case SDLK_d: chip8.keys[0x9] = pressed; break;
                case SDLK_f: chip8.keys[0xE] = pressed; break;
                case SDLK_z: chip8.keys[0xA] = pressed; break;
                case SDLK_x: chip8.keys[0x0] = pressed; break;
                case SDLK_c: chip8.keys[0xB] = pressed; break;
                case SDLK_v: chip8.keys[0xF] = pressed; break;
                }
            }
        }

        uint16_t opcode = (chip8.memory[chip8.pc] << 8) | chip8.memory[chip8.pc + 1];
        cycle(&chip8, opcode);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 64; x++) {
                if (chip8.display[y * 64 + x]) {
                    SDL_Rect pixel = { x * 10, y * 10, 10, 10 };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        SDL_RenderPresent(renderer);

        // Timers tick at 60Hz
        uint32_t now = SDL_GetTicks();
        if (now - last_timer_tick >= 16) {
            if (chip8.delay_timer > 0) chip8.delay_timer--;
            if (chip8.sound_timer > 0) chip8.sound_timer--;
            last_timer_tick = now;
        }

        SDL_Delay(2); // slow down execution speed
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}