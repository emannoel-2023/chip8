#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "SDL.h"

// SDL CONTAINER OBJETO
typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
} sdl_t;


// EMULADOR CONFIGURACAO OBJETO
typedef struct {
	uint32_t window_width;
	uint32_t window_height;
	uint32_t fg_color;
	uint32_t bg_color;
	uint32_t scale_factor;
	bool pixel_outlines;
} config_t;


// ESTADOS DO EMULADOR
typedef enum {
	QUIT,
	RUNNING,
	PAUSED,


} emulator_state_t;

// CHIP8 formato de instrucao
typedef struct {
	uint16_t opcode;
	uint16_t NNN; // 12 bit de endereco/constante
	uint8_t NN; //8 bit constate
	uint8_t N; //8 bit constate
	uint8_t X; //4 bit registrador indetificador
	uint8_t Y; //4 bit registrador indentificador
}instruction_t;


//CHIP8 MAQUINA OBJETO
typedef struct {
	emulator_state_t state;
	uint8_t ram[4096];
	bool display[64*32]; // resolucao original do chip8 display = &ram[0xF00]; display[10]
	uint16_t stack[12]; // subrotina do stack
	uint16_t *stack_ptr;
	uint8_t V[16]; // registrador data V0-VF
	uint16_t I; // Idex do registrador
	uint16_t PC; // contador de programas
	uint8_t delay_timer; // decrementa a 60hz quando >0 
	uint8_t sound_timer; // decrementa a 60hz e toca um tom  quando >0
	bool keypad[16]; // hexadecimal do teclado 0x0-0xF	
	const char *rom_name; // rom rodando
	instruction_t inst; //instrucao em execucao no momento
} chip8_t;


// inicio SDL
bool init_sdl(sdl_t *sdl, const config_t config){
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) !=0){
		SDL_Log("Impossivel iniciar o SDL subsystems! %s\n", SDL_GetError());
		return false;
	}

	sdl->window = SDL_CreateWindow("Chip8 do barones", SDL_WINDOWPOS_CENTERED,
					SDL_WINDOWPOS_CENTERED,
					config.window_width * config.scale_factor,
					config.window_height * config.scale_factor,
					0);
	if (!sdl->window){
		SDL_Log("Impossivel criar a janela SDL %s\n", SDL_GetError());
		return false;
	}

	sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
	if (!sdl->renderer){
		SDL_Log("Impossivel criar a renderizador SDL %s\n", SDL_GetError());
		return false;	
	}

	return true; //sucesso
}

// configuracao inicial do emulador apartir dos argumentos passados
bool set_config_from_args(config_t *config, const int argc, char **argv) {
	
	//valor padrao
	*config = (config_t) {
		.window_width = 64, // resolucao X original chip8 
		.window_height = 32, // resolucao Y original chip8
		.fg_color = 0xFFFFFFFF, // amarelo
		.bg_color = 0x000000FF, // preto
		.scale_factor = 20, 	// resolucao padrao 
		.pixel_outlines = true,
	};
	
	// substituir padroes dependendo do que foi passado no terminal
	for(int i=1; i<argc; i++) {
		(void)argv[i];
		// ...
	};

	return true;

}

bool init_chip8(chip8_t *chip8,const char rom_name[]){
	const uint32_t entry_point = 0x200; // carregando as roms do chip8 no 0x200
	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0  11110000
 		0x20, 0x60, 0x20, 0x20, 0x70,   // 1  10010000
        	0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2  10010000
        	0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3  10010000
        	0x90, 0x90, 0xF0, 0x10, 0x10,   // 4  11110000
        	0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        	0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        	0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        	0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        	0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        	0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        	0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        	0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        	0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        	0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        	0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
	};
	//carregar fonte
	memcpy(&chip8->ram[0], font, sizeof(font));
	//carregar rom
	FILE *rom = fopen(rom_name, "rb");
	if(!rom){
		SDL_Log("arquivo ROM %s eh invalido ou nao existe\n", rom_name);
		return false;
		
	}

	//get/check arquivo rom
	fseek(rom, 0, SEEK_END);
	const size_t rom_size = ftell(rom);
	const size_t max_size = sizeof chip8->ram - entry_point;
	rewind(rom);

	if (rom_size > max_size) {
		SDL_Log("arquivo de rom %s eh muito grande, tamanho da rom: %llu, tamanho maximo permitido: %llu\n", rom_name, (unsigned long long)rom_size, (unsigned long long)max_size);	
		return false;

	}

	if(fread(&chip8->ram[entry_point], rom_size,1,rom) != 1) {
		SDL_Log("nao consigo ler o arquivo rom %s na memoria do chip8\n", rom_name);
		return false;

	}

	fclose(rom);

	// set padrao da maquina chip8
	chip8->state = RUNNING;
	chip8->PC = entry_point; // comencando a rom neste entry point
	chip8->rom_name = rom_name;
	chip8->stack_ptr = &chip8->stack[0];
	return true;

}

//fim do programa
void final_cleanup(const sdl_t sdl){
	SDL_DestroyRenderer(sdl.renderer);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit(); // desligando SDL subsystems
}


// limpeza da tela SLD para o background
void clear_screen(const sdl_t sdl, const config_t config){
	const uint8_t r = (config.bg_color >> 24) & 0xFF;
	const uint8_t g = (config.bg_color >> 16) & 0xFF;
	const uint8_t b = (config.bg_color >>  8) & 0xFF;
	const uint8_t a = (config.bg_color >>  4) & 0xFF;


	SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
	SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl,const config_t config, const chip8_t chip8){
	SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
	
	const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
	const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
	const uint8_t fg_b = (config.fg_color >>  8) & 0xFF;
	const uint8_t fg_a = (config.fg_color >>  4) & 0xFF;

	const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
	const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
	const uint8_t bg_b = (config.bg_color >>  8) & 0xFF;
	const uint8_t bg_a = (config.bg_color >>  4) & 0xFF;

	// loop through display pixels, draw a rectangle per pixel to the SDL window
	for (uint32_t i = 0; i < sizeof chip8.display; i++){
		rect.x = (i % config.window_width) * config.scale_factor;
		rect.y = (i / config.window_width) * config.scale_factor;
		
		if (chip8.display[i]){
			SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
			SDL_RenderFillRect(sdl.renderer, &rect);

			//if user requested drawing pixel outlines, draw those here
			if(config.pixel_outlines){
				SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
				SDL_RenderDrawRect(sdl.renderer, &rect);
			}
		} else{
			SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
			SDL_RenderFillRect(sdl.renderer, &rect);

		}
	}

	SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8){
	SDL_Event event;

	while (SDL_PollEvent(&event)){
		switch (event.type) {
			case SDL_QUIT:
				// Sair da janela; Fim do programa
				chip8->state = QUIT;
				return;
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym){
					case SDLK_ESCAPE:
						//escape key; sair da janela e fechar programa
						chip8->state = QUIT;
						return;
					case SDLK_SPACE:
						//barra de espoaço
						if(chip8->state==RUNNING){
							chip8->state = PAUSED;
							puts("======PAUSED======");
						}
						else{
							chip8->state = RUNNING;

						}
						return;
							
					default:
						break;

				}

			case SDL_KEYUP:
				break;
			default:
				break;
		}		
	}

}


#ifdef DEBUG
void print_debug_info(chip8_t *chip8) {
	printf("Address: 0x%04X, opcode: 0x%04X Desc: ",
	chip8->PC-2, chip8->inst.opcode);

	switch ((chip8->inst.opcode >>12) & 0x0F) {
		case 0x00:
			if(chip8->inst.NN == 0xE0){
				//0xE0: limpar a tela
				printf("clear screen\n");
			} else if (chip8->inst.NN == 0xEE){
				//0x0EE: Return form subroutine
				//set program counter to last address on subroutine stack ("pop" it off the stack)
				//so that next opcode will gotten from that address
				printf("Return from subroutine to address 0x%04X\n",
	   				*(chip8->stack_ptr - 1));	
			}else{
				printf("unimplemented opcode\n");
			}
			break;
		case 0x01:
			//0x1NNN: jump to address NNN
			printf("jump to address NNN (0x%04X)\n",
				chip8->inst.NNN);
			break;
		case 0x02:
			//0x2NNN: call subroutine at NNN
			//store current address to return to on subroutine stack("push" it on the stack)
			//and set program counter to subroutine address so that the nest opcode
			//is gotten from here.
			*chip8->stack_ptr++ = chip8->PC;
			chip8->PC = chip8->inst.NNN;
			break;

		case 0x03:
			//0x3XNN: check if VX == NN, if so, skip the next instruction
			printf("check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n",
	  			chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.NN);
			break;
		case 0x04:
			//0x4XNN: check if VX != NN, if so, skip the next instruction
				printf("check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n",
	  			chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.NN);

			break;
		case 0x05:
			// 0x5XY0: check if VX != NN, if so, skip the next insctruction
			printf("check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n",
	  			chip8->inst.X, chip8->V[chip8->inst.X],
	  			chip8->inst.Y, chip8->V[chip8->inst.Y]);
			break;
		case 0x06:
			//0x6XNN set register VX to NN
			printf("set register V%X to NN (0x%02X)\n",
				chip8->inst.X, chip8->inst.NN);
			break;
		case 0x07:
			//0x7XNN set register VX += NN
			printf("set register V%X (0x%02X)+= NN (0x%02X). Result: 0x%02X\n",
	  			chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN,chip8->V[chip8->inst.X]+ chip8->inst.NN);
			break;

		case 0x08:
			switch(chip8->inst.N) {
				case 0:
					//0x8XY0: set register VX = VY
					printf("set register V%X = V%X (0x%02X)\n",
						chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
					break;
				case 1:
					//0x8XY1: set register VX |= VY
					printf("set register V%X (0x%02X) |= V%X (0x%02X): result: 0x%02X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->inst.Y, chip8->V[chip8->inst.Y],
						chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
					break;
				case 2:
					//0x8XY2: set register VX &= VY
					printf("set register V%X (0x%02X) &= V%X (0x%02X): result: 0x%02X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->inst.Y, chip8->V[chip8->inst.Y],
						chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
					break;
				case 3:
					//0x8XY3: set register VX ^= VY
					printf("set register V%X (0x%02X) ^= V%X (0x%02X): result: 0x%02X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->inst.Y, chip8->V[chip8->inst.Y],
						chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
					break;
				case 4:
					//0x8XY4: set register VX += VY set VF to 1 if carry
					printf("set register V%X (0x%02X) += V%X (0x%02X),VF= 1 if carry; result: 0x%02X, VF=%X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->inst.Y, chip8->V[chip8->inst.Y],
						chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
						((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y])>255));
					break;
				case 5:
					//0x8XY5: set register VX -= VY, set VF to 1 if there is not a borrow (result is positive/0)
					printf("set register V%X (0x%02X) -= V%X (0x%02X),VF= 1 if no borrow; result: 0x%02X, VF=%X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->inst.Y, chip8->V[chip8->inst.Y],
						chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
						(chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
					break;
				case 6:
					//0x8XY6: set register VX >>=1, store shifted off bit in VF
					printf("set register V%X (0x%02X) >>= 1, VF= shifted off bit (%X); 0x%02X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						chip8->V[chip8->inst.X] & 1,
						chip8->V[chip8->inst.X] >> 1);
					break;
				case 7:
					//0x8XY7 set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive/0)
					printf("set register V%X = V%X (0x%02X) - V%X (0x%02X),VF= 1 if no borrow; result: 0x%02X, VF=%X\n",
						chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y], 
						chip8->inst.X, chip8->V[chip8->inst.X],
						chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
						(chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
					break;
				case 0xE:
					//0x8XYE: set register VX <<= 1, store shifted off bit in VF
					printf("set register V%X (0x%02X) <<= 1, VF= shifted off bit (%X); 0x%02X\n",
						chip8->inst.X, chip8->V[chip8->inst.X], 
						(chip8->V[chip8->inst.X] & 0x80) >> 7,
						chip8->V[chip8->inst.X] << 1);
					break;
				default:
					//wrong/unimplemented opcode
					break;

			}
			break;

		case 0x09:
			// 0x9XY0: check if VX != VY: skip next instruction of so
			printf("check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n",
	  			chip8->inst.X, chip8->V[chip8->inst.X],
	  			chip8->inst.Y, chip8->V[chip8->inst.Y]);
			break;
		case 0x0A:
			//0xANNN set index register I to NNN
			printf("set I to NNN (0x%04X)\n",chip8->inst.NNN);
			break;
		case 0x0B:
			//0xBNN: jump to V0 + NNN
			printf("set pc to V0 (0x%02X) + NNN (0x%04X); result pc = 0x%04X\n",
				chip8->V[0], chip8->inst.NNN, chip8->V[0]+chip8->inst.NNN);
			break;

		case 0x0D:
			printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X)"
	  			"from memory location I (0x%04X). Set VF= 1 if any pixels are turned off\n", 
	  			chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y,
	  			chip8->V[chip8->inst.Y],chip8->I);
			
			break;
		
		default:
			printf("Unimplementd opcode\n");
			break;

	}		
}
#endif

// instrucao 1 de emulacao chip8
void emulate_instruction(chip8_t *chip8, const config_t config){
	// pega o proximo opcode da ram
	chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1];
	chip8->PC +=2; // pre incremento para o proximo opcode
	
	// Fill out current instruction format
	chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
	chip8->inst.NN = chip8->inst.opcode & 0x0FF;
	chip8->inst.N = chip8->inst.opcode & 0x0F;
	chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
	chip8->inst.Y = (chip8->inst.opcode >>4) & 0x0F;

#ifdef DEBUG
	print_debug_info(chip8);
#endif

	//emulate opcode
	switch ((chip8->inst.opcode >>12) & 0x0F) {
		case 0x00:
			if(chip8->inst.NN == 0xE0){
				//0xE0: limpar a tela
				memset(&chip8->display[0],false,sizeof chip8->display);
			} else if (chip8->inst.NN == 0xEE){
				//0x0EE: Return form subroutine
				//set program counter to last address on subroutine stack ("pop" it off the stack)
				//so that next opcode will gotten from that address
				chip8->PC = *--chip8->stack_ptr;	
			} else {
				//unimplemented/invalid opcode, may be 0xNNN for caling machine code routine for RCA1802
			}
			break;
		case 0x01:
			//0x1NNN: jump to address NNN
			chip8->PC = chip8->inst.NNN; // set program counte so that next opcode is from NNN
			break;
		case 0x02:
			//0x2NNN: call subroutine at NNN
			//store current address to return to on subroutine stack("push" it on the stack)
			//and set program counter to subroutine address so that the nest opcode
			//is gotten from here.
			*chip8->stack_ptr++ = chip8->PC;
			chip8->PC = chip8->inst.NNN;
			break;
		case 0x03:
			//0x3XNN: check if VX == NN, if so, skip the next instruction
			if(chip8->V[chip8->inst.X]== chip8->inst.NN)
				chip8->PC += 2; //skip next opcode/instruction
			break;
		case 0x04:
			//0x3XNN: check if VX != NN, if so, skip the next instruction
			if(chip8->V[chip8->inst.X] != chip8->inst.NN)
				chip8->PC += 2; //skip next opcode/instruction
			break;
		case 0x05:
			// 0x5XY0: check if VX != NN, if so, skip the next insctruction
			if(chip8->inst.N != 0) break;
			if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
				chip8->PC +=2; //skip next opcode/instruction
			break;
		case 0x06:
			//0x6XNN set register VX to NN
			chip8->V[chip8->inst.X] = chip8->inst.NN;
			break;
		case 0x07:
			//0x7XNN set register VX += NN
			chip8->V[chip8->inst.X] += chip8->inst.NN;
			break;
		case 0x08:
			switch(chip8->inst.N) {
				case 0:
					//0x8XY0: set register VX = VY
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
					break;
				case 1:
					//0x8XY1: set register VX |= VY
					chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
					break;
				case 2:
					//0x8XY2: set register VX &= VY
					chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
					break;
				case 3:
					//0x8XY3: set register VX ^= VY
					chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
					break;
				case 4:
					//0x8XY4: set register VX += VY set VF to 1 if carry
					if ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y])>255)
						chip8->V[0xF] = 1;
					chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
					break;
				case 5:
					//0x8XY5: set register VX -= VY, set VF to 1 if there is not a borrow (result is positive/0)
					if(chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X])
						chip8->V[0xF] = 1;
					chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
					break;
				case 6:
					//0x8XY6: set register VX >>=1, store shifted off bit in VF
					chip8->V[0xF] = chip8->V[chip8->inst.X] & 1;
					chip8->V[chip8->inst.X] >>= 1;
					break;
				case 7:
					//0x8XY7 set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive/0)
					if(chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y])
						chip8->V[0xF] = 1;
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
					break;
				case 0xE:
					//0x8XYE: set register VX <<= 1, store shifted off bit in VF
					chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
					chip8->V[chip8->inst.X] <<= 1;
					break;
				default:
					//wrong/unimplemented opcode
					break;

			}
			break;
		case 0x09:
			// 0x9XY0: check if VX != VY: skip next instruction of so
			if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
				chip8->PC += 2;
			break;
		case 0x0A:
			//0xANNN set index register I to NNN
			chip8->I = chip8->inst.NNN;
			break;
		case 0x0B:
			//0xBNN: jump to V0 + NNN
			chip8->PC = chip8->V[0] + chip8->inst.NNN;
			break;
		case 0x0D:
			// 0zDXYN : draw N height sprite at coords X,Y; Read from memory location I;
			// screen pixels are XOR'd with sprite bits,
			// VF (carry flag) is set if any, screen pixels are set off; this is useful 
			// for collision detection or reasons
			uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
			uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
			const uint8_t orig_X = X_coord; // original X value

			chip8->V[0xF] = 0; // initialize carry flag to 0

			for (uint8_t i=0; i<chip8->inst.N; i++){
				//get next byte/row of sprite data
				const uint8_t sprite_data = chip8->ram[chip8->I + i];
				X_coord = orig_X; // reset X for next row to draw
				for (int8_t j= 7; j >=0; j--){
					//if sprite pixel/bit is on and display pixel is on, set carry flag
					bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];
					const bool sprite_bit = (sprite_data & (1 << j));
					if (sprite_bit && *pixel){
						chip8->V[0xF] = 1;
					}
					//xor display pixel with sprite pixel/bit
					*pixel ^= sprite_bit;

					//stop drawing this row if hit right edge of screen
					if (++X_coord >= config.window_width) break;
				}
				//stop drawing entire sprite if hit bottom edge of screen
				//
				if (++Y_coord >= config.window_height) break;
			}
			break;
		default:
			break;

	}
}	


int main(int argc, char **argv) {
	
	if(argc <2) {
		fprintf(stderr, "Usage: %s <rom_name>\n",argv[0]);
		exit(EXIT_FAILURE);
	}

	config_t config = {0};
	if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

	// Iniciando o SDL
	sdl_t sdl = {0};
	if (!init_sdl(&sdl, config)) exit(EXIT_FAILURE);
	
	// Iniciando maquina chip8
	chip8_t chip8 = {0};
	const char *rom_name = argv[1];
	if (!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);


	// limpeza inicial da tela para a cor do background
	clear_screen(sdl, config);


	// main loop do emulador
	while(chip8.state != QUIT){
		//input do usuario
		handle_input(&chip8);
		if (chip8.state == PAUSED) continue; 
		// get time()
		// instrucoes do emulador de chip8
		emulate_instruction(&chip8, config);
		// Get_time() elapsed since last get_time()
		SDL_Delay(16);
		update_screen(sdl, config, chip8);

	}

	// final cleanup
	final_cleanup(sdl);

	exit(EXIT_SUCCESS);
}
