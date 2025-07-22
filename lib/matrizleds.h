#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "matriz_leds.pio.h"

#define NUM_PIXELS 25       //Definição do número de LEDs da matriz 5X5
#define LEDS_PIN 7          //Definição da porta GPIO da matriz de LEDs 5X5

// Função para converter os parâmetros r, g, b em um valor de 32 bits
uint32_t matrix_rgb(double b, double r, double g);
    
// Função para formar os desenhos na matriz de LEDs 5x5
void desenhos(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b);