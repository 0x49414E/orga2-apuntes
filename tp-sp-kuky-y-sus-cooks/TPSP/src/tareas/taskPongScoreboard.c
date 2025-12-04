#include "task_lib.h"

#define WIDTH TASK_VIEWPORT_WIDTH
#define HEIGHT TASK_VIEWPORT_HEIGHT

#define SHARED_SCORE_BASE_VADDR (PAGE_ON_DEMAND_BASE_VADDR + 0xF00)
#define CANT_PONGS 3
#define CANT_JUGADORES CANT_PONGS*2

void task(void) {
	screen pantalla;

	uint16_t colores[] = {
		0x34, 
		0x9d, 
		0x15, 
	}; 	
	
	// ¿Una tarea debe terminar en nuestro sistema?
	while (true)
	{
	// Completar:
	// - Pueden definir funciones auxiliares para imprimir en pantalla
	// - Pueden usar `task_print`, `task_print_dec`, etc.
		//uint32_t* arreglo_de_puntajes = (uint32_t*) (SHARED_SCORE_BASE_VADDR);

		task_print(pantalla, "Scoreboard: ", 10, 2, C_FG_LIGHT_GREEN);

		for (int i = 0; i < CANT_PONGS; i++) {
			uint32_t* jugadores = (uint32_t*) (SHARED_SCORE_BASE_VADDR + (i * sizeof(uint32_t)*2)); 
			uint32_t posicion_y = 5 + (i * 5);
			task_print_dec(pantalla, jugadores[0], 2, 10, posicion_y, colores[i] >> 4);
			task_print_dec(pantalla, jugadores[1], 2, 20, posicion_y, colores[i] & 0x0F);
		}
	
		syscall_draw(pantalla);
	}
}
