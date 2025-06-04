#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "SDL.h"


typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
} sdl_t;

typedef struct {
	uint32_t window_width;
	uint32_t window_height;
	uint32_t fg_color;
	uint32_t bg_color;
} config_t;

// inicio SDL
bool init_sdl(sdl_t *sdl, const config_t config){
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) !=0){
		SDL_Log("Impossivel iniciar o SDL subsystems! %s\n", SDL_GetError());
		return false;
	}

	sdl->window = SDL_CreateWindow("Chip8 do barones", SDL_WINDOWPOS_CENTERED,
					SDL_WINDOWPOS_CENTERED,
					config.window_width,config.window_height,
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
		.fg_color = 0xFFFF00FF, // amarelo
		.bg_color = 0x00000000, // preto
	};
	
	// substituir padroes dependendo do que foi passado no terminal
	for(int i=1; i<argc; i++) {
		(void)argv[i];
		// ...
	};

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

void update_screen(const sdl_t sdl){
	SDL_RenderPresent(sdl.renderer);
}

int main(int argc, char **argv) {
	config_t config = {0};
	if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

	// Iniciando o SDL
	sdl_t sdl = {0};
	if (!init_sdl(&sdl, config)) exit(EXIT_FAILURE);
	
	// limpeza inicial da tela para a cor do background
	clear_screen(sdl, config);


	// main loop do emulador
	while(true){
		SDL_Delay(16);
		update_screen(sdl);

	}

	// final cleanup
	final_cleanup(sdl);

	exit(EXIT_SUCCESS);
}
