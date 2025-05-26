#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INST_COUNT (34)
#define FONT_START (0x50)
#define NNN(i) (i & 0x0FFF)
#define NIBBLE(i) (i & 0x000F)
#define NIBBLE2(i) (i & 0x00F0)
#define NIBBLE3(i) (i & 0x0F00)
#define NIBBLE4(i) (i & 0xF000)
#define VX(i) ((i >> 8) & 0x0F)
#define VY(i) ((i >> 4) & 0x0F)
#define KK(i) (i & 0x0F)
#define VF (15)


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

typedef struct
{
    uint8_t memory[4096]; //mem
    uint8_t V[16]; //registers
    uint16_t I; //index reg
    uint16_t pc; //program coutner
    uint8_t sp; //stack pointer
    uint16_t stack[16];
    uint8_t display[64*32];
    uint8_t keys[16];
    uint8_t delay_timer;
    uint8_t sound_timer;
} chip8_t;

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
    or, //10
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

static inline void clr(chip8_t* chip8, uint16_t opcode){
    for (int i = 0; i < 2048; i++)
        chip8->V[i] = 0;
}

static inline void ret(chip8_t* chip8, uint16_t opcode) {chip8->pc = chip8->stack[chip8->sp];}

static inline void jpaddr(chip8_t* chip8, uint16_t opcode) {chip8->pc = NNN(opcode);}

static inline void call(chip8_t* chip8, uint16_t opcode){
    chip8->sp++;
    chip8->stack[chip8->sp] = chip8->pc;
    chip8->pc = NNN(opcode);
}

static inline void se(chip8_t* chip8, uint16_t opcode) {chip8->pc = (chip8->V[VX(opcode)] == KK(opcode)) ? chip8->pc + 2 : chip8->pc;}

static inline void sne(chip8_t* chip8, uint16_t opcode) {chip8->pc = (chip8->V[VX(opcode)] != KK(opcode)) ? chip8->pc + 2 : chip8->pc;}

static inline void sre(chip8_t* chip8, uint16_t opcode) {chip8->pc = (chip8->V[VX(opcode)] == chip8->V[VY(opcode)]) ? chip8->pc + 2 : chip8->pc;}

static inline void load(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] = KK(opcode);}

static inline void add(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] += KK(opcode);}

static inline void loadr(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] = chip8->V[VY(opcode)];}

static inline void or(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] = chip8->V[VY(opcode)] | chip8->V[VX(opcode)];}

static inline void and(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] = chip8->V[VY(opcode)] & chip8->V[VX(opcode)];}

static inline void xor(chip8_t* chip8, uint16_t opcode) {chip8->V[VX(opcode)] = chip8->V[VY(opcode)] ^ chip8->V[VX(opcode)];}

static inline void addxy(chip8_t* chip8, uint16_t opcode) {
    chip8->V[VX(opcode)] += chip8->V[VY(opcode)];
    chip8->V[VF] = ((chip8->V[VX(opcode)] + chip8->V[VY(opcode)]) > 255) ? 1 : 0;
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

static inline void shl(chip8_t* chip8, uint16_t opcode){
    chip8->V[VF] = chip8->V[VX(opcode)] & 0x80; //if lsb is one vf will be one
    chip8->V[VX(opcode)] = chip8->V[VX(opcode)] << 1;
}

static inline void loadI(chip8_t* chip8, uint16_t opcode){
    chip8->I=NNN(opcode);
}

static inline void jmp(chip8_t* chip8, uint16_t opcode){
    chip8->pc = chip8->V[0] + NNN(opcode);
}

static inline void rnd(chip8_t* chip8, uint16_t opcode){
    chip8->V[VX(opcode)] = rand() % 256 + KK(opcode);
}

static inline void draw(chip8_t* chip8, uint16_t opcode){
    uint8_t x_pos = chip8->V[VX(opcode)] % 64;
    uint8_t y_pos = chip8->V[VY(opcode)] % 32;
    for(int i = 0; i < NIBBLE(opcode); i++){
        if(chip8->display[x_pos*y_pos+i]) chip8->V[VF] = 1;
        chip8->display[x_pos*y_pos+i] ^= chip8->memory[chip8->I+i]; 
        //this could go wrong if xposypos are like 62 30 and the nibble is too large
    }
}

static inline void skip(chip8_t* chip8, uint16_t opcode){if(chip8->keys[chip8->V[VX(opcode)]]) chip8->pc += 2;}

static inline void skip_not(chip8_t* chip8, uint16_t opcode){if (!(chip8->keys[chip8->V[VX(opcode)]])) chip8->pc += 2;}

static inline void loadDT(chip8_t* chip8, uint16_t opcode){chip8->V[VX(opcode)] = chip8->delay_timer;}

static inline void loadKey(chip8_t* chip8, uint16_t opcode){
    uint8_t key;
    scanf("%d", key);
    if (key > 15) perror("NOT ALLOWED KEY");
    // {UPDATE THIS}
    chip8->V[VX(opcode)] = key;
}

static inline void setDT(chip8_t* chip8, uint16_t opcode){chip8->delay_timer = chip8->V[VX(opcode)];}

static inline void setST(chip8_t* chip8, uint16_t opcode){chip8->sound_timer = chip8->V[VX(opcode)];}

static inline void addI(chip8_t* chip8, uint16_t opcode){chip8->I += chip8->V[VX(opcode)];}

static inline void loadF(chip8_t* chip8, uint16_t opcode){chip8->I = chip8->V[VX(opcode)]*5 + FONT_START;}

static inline void loadB(chip8_t* chip8, uint16_t opcode){
    uint8_t val = chip8->V[VX(opcode)];
    chip8->memory[chip8->I+2] = val % 10; //lsb 
    val /= 10;
    chip8->memory[chip8->I+1] = val % 10; //lsb 
    val /= 10;
    chip8->memory[chip8->I] = val % 10;
}

static inline void readRegs(chip8_t* chip8, uint16_t opcode){
    for(int i = 0; i <= XV(opcode); i++)
        chip8->memory[chip8->I+i] = chip8->V[i];
}

static inline void writeRegs(chip8_t* chip8, uint16_t opcode){
    for(int i = 0; i <= XV(opcode); i++)
        chip8->V[i] = chip8->memory[chip8->I+i];
}

// ## definitely could have went a better way for decoding
//Already wrote all the functions and I just need the index for the funcptr, the opcode
//is used properly in each function
static inline int decode(uint16_t opcode){
    switch NIBBLE4(opcode) {
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
            switch NIBBLE(opcode){
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
        case 0xF:
            switch(NIBBLE2(opcode) & NIBBLE(opcode)){
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

void cycle(chip8_t* chip8, uint16_t opcode){
    //fetch (get the opcode)
    //execute the instruction
    //make any additional changes as needy
    uint8_t inst_index = decode(opcode);
    chip8->pc+=2;
    instructions[inst_index](chip8, opcode); //execute step
    if(chip8->delay_timer > 0) chip8->delay_timer--;
    if(chip8->sound_timer > 0) chip8->sound_timer--;
}

void init(chip8_t* chip8){
    //clear all arrays and also load font into memory. 
    memset(chip8->memory, 0, 4096);
    memset(chip8->V, 0, 16);
    memset(chip8->display, 0, 2048);
    memset(chip8->keys, 0, 16);
    memset(chip8->stack, 0, 16);
    memcpy(chip8->memory, fontset, sizeof(fontset));
}

int main(int argc, char* argv){


    chip8_t chip8;
    init(&chip8);

}