#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"


// inicio SDL
bool init_sdl(void){
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) !=0){
		SDL_Log("Impossivel iniciar o SDL subsystems! %s\n", SDL_GetError());
		return false;
	}

	return true; //sucesso
}

//fim do programa
void final_cleanup(void){
	SDL_Quit(); // desligando SDL subsystems
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	// Iniciando o SDL
	if (!init_sdl()) exit(EXIT_FAILURE);
	
	// final cleanup
	final_cleanup();

	exit(EXIT_SUCCESS);
}
