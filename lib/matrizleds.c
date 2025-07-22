#include "matrizleds.h"

#define NUM_PIXELS 25       //Definição do número de LEDs da matriz 5X5
#define LEDS_PIN 7          //Definição da porta GPIO da matriz de LEDs 5X5

// Função para converter os parâmetros r, g, b em um valor de 32 bits
uint32_t matrix_rgb(double b, double r, double g){
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}
    
// Função para formar os desenhos na matriz de LEDs 5x5
void desenhos(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){
    // O loop aciona cada LED da matriz com base em um valor de cor 
    for (int i = 0; i < NUM_PIXELS; i++){
        // Determinação da cor de cada LED
        uint32_t valor_led = matrix_rgb(desenho[24 - i]*r, desenho[24 - i]*g, desenho[24 - i]*b);
        // O valor é enviado ao LED para ser exibido
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}