// Inclusão das bibliotecas necessárias ao código
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" 
#include "lwip/pbuf.h"       
#include "lwip/tcp.h"        
#include "lwip/netif.h"    
#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include "lib/matrizleds.h"
#include "matriz_leds.pio.h"
#include <math.h>

// Definição de constantes globais
#define WIFI_SSID "POCO F5"
#define WIFI_PASSWORD "123456789"
#define I2C_PORT i2c0             
#define I2C_SDA 0                  
#define I2C_SCL 1                  
#define SEA_LEVEL_PRESSURE 101325.0 
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C
#define LED_VERDE 11
#define LED_VERMELHO 13
#define LED_AZUL 12
#define BOTAO_A 5
#define BOTAO_B 6
#define BUZZER 21
#define TEMP_MIN 22.0
#define TEMP_MAX 28.0
#define HUM_MIN 40.0
#define HUM_MAX 70.0
#define PRESS_MIN 90.0
#define PRESS_MAX 1020.0

// Definição de variáveis globais e flags utilizadas
static volatile PIO pio = pio0;
static volatile uint offset;
static volatile uint sm;
static volatile uint32_t valor_led;
static volatile uint32_t ultimo_tempo = 0;
static volatile bool exibir_resultado = false;
static volatile bool bmp280_ativo = false;
ssd1306_t ssd;  
static volatile float prev_temp;
static volatile float prev_press;
static volatile float prev_hum; 
volatile float humi_min = 40.0;
volatile float humi_max = 60.0;
volatile float temp_min = 22.0;
volatile float temp_max = 28.0;
volatile float press_min = 1005.0;
volatile float press_max = 1020.0;
int32_t pressure;
double altitude;
int32_t temperature; 
AHT20_Data data;
int32_t raw_temp_bmp;
int32_t raw_pressure;

// Vetores usados para criar os desenhos na matriz de LEDs
double desenho1[NUM_PIXELS] = {0.0, 0.5, 0.0, 0.5, 0.0,
                               0.0, 0.5, 0.0, 0.5, 0.0,
                               0.0, 0.5, 0.0, 0.5, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.5, 0.0, 0.5, 0.0}; 

double desenho2[NUM_PIXELS] = {0.0, 0.0, 0.5, 0.0, 0.0,
                               0.0, 0.0, 0.5, 0.0, 0.0,
                               0.0, 0.0, 0.5, 0.0, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.0, 0.5, 0.0, 0.0};

double desenho3[NUM_PIXELS] = {0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0,
                               0.5, 0.5, 0.5, 0.5, 0.5,
                               0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0};

// Função para calcular a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure){
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// Função para utilização de botões por meio de interrupções e debounce
static void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_us_since_boot(get_absolute_time());

    if(tempo_atual - ultimo_tempo > 200000){
        if(!gpio_get(BOTAO_A)){
            exibir_resultado = !exibir_resultado;
            ultimo_tempo = tempo_atual;
        } else if (!gpio_get(BOTAO_B)){
            humi_min = HUM_MIN;
            humi_max = HUM_MAX;
            temp_min = TEMP_MIN;
            temp_max = TEMP_MAX;
            press_min = PRESS_MIN;
            press_max = PRESS_MAX;
            ultimo_tempo = tempo_atual;
        }
    }
}

// Função para acionamento do buzzer
void tocar_buzzer(int frequencia, int duracao)
{
    for (int i = 0; i < duracao * 1000; i += (1000000 / frequencia) / 2)
    {
        gpio_put(BUZZER, 1);
        sleep_us((1000000 / frequencia) / 2);
        gpio_put(BUZZER, 0);
        sleep_us((1000000 / frequencia) / 2);
    }
}

//Função para inicializar os periféricos da placa e demais estruturas utilizadas
void inicializar_perif(){

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line                                                   // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Determinação de variáveis necessárias para funcionamento da matriz de LEDs
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &leds_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    uint32_t valor_led;
    leds_matrix_program_init(pio, sm, offset, LEDS_PIN);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, 0);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, 0);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, 0);
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

}

//Esqueleto do html usado
const char HTML_BODY[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Estação Meteorológica</title>"
"<style>"
"body { font-family: Arial, sans-serif; text-align: center; padding: 20px; background-color: #f9f9f9; }"
"h1 { color: #005f99; }"
"p { font-size: 16px; margin: 10px 0; }"
"input, button { font-size: 14px; margin: 5px; padding: 6px 10px; }"
"</style>"
"<script>"
"function atualizar() {"
"  fetch('/clima')"
"    .then(res => res.json())"
"    .then(data => {"
"      document.getElementById('sensor-type').innerText = data.sensor === 0 ? 'BMP280' : 'AHT20';"
"      if(data.sensor == 0){"
"       document.getElementById('umidade').innerText = data.umidade.toFixed(2) + ' %% ';"
"       document.getElementById('tempAHT').innerText = data.tempAHT.toFixed(2) + ' °C ';"
"       document.getElementById('aht-container').style.display = 'block';"
"       document.getElementById('bmp-container').style.display = 'none';"
"       } else {"
"       document.getElementById('pressao').innerText = data.pressao.toFixed(2) + ' kPa ';"
"       document.getElementById('tempBMP').innerText = data.tempBMP.toFixed(2) + ' °C ';"
"       document.getElementById('aht-container').style.display = 'none';"
"       document.getElementById('bmp-container').style.display = 'block';"     
"}"
"}"
"}"");"
"}"
"setInterval(atualizar, 1000);"
"</script></head><body>"
"<h1>Estação Meteorológica</h1>"
"<p>Sensor ativo: <strong id='sensor-type'>---</strong></p>"
"<div id='aht-container' style='display:none'>"
"<p>Temperatura (AHT20): <strong id='tempAHT'>---</strong></p>"
"<p>Umidade: <strong id='umidade'>---</strong></p>"
"</div>"
"<div id='bmp-container' style='display:none'>"
"<p>Temperatura (BMP280): <strong id='tempBMP'>---</strong></p>"
"<p>Pressão: <strong id='pressao'>---</strong></p>"
"</div>"
"<hr>"
"<p>Definir limite mínimo de temperatura AHT (22°C a 25°C):</p>"
"<input type='number' id='minimoInput'><button onclick='enviarMin()'>Atualizar Mínimo</button>"
"<p>Definir limite máximo de temperatura AHT(26°C a 28°C):</p>"
"<input type='number' id='maximoInput'><button onclick='enviarMax()'>Atualizar Máximo</button>"
"<p>Definir limite mínimo de umidade (40% a 50%):</p>"
"<input type='number' id='minimoInput'><button onclick='enviarMin()'>Atualizar Mínimo</button>"
"<p>Definir limite máximo de umidade (60% a 70%):</p>"
"<input type='number' id='maximoInput'><button onclick='enviarMax()'>Atualizar Máximo</button>"
"<script>"
"function enviarMin() {"
"  var val = document.getElementById('minimoInput').value;"
"  fetch('/minimun/' + val);"
"}"
"function enviarMax() {"
"  var val = document.getElementById('maximoInput').value;"
"  fetch('/maximun/' + val);"
"}"
"</script></body></html>";

struct http_state
{
    char response[4096];
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

        if (strstr(req, "GET /minimun/temp/"))
        {
            int valor = atoi(strstr(req, "/minimun/temp/") + strlen("/minimun/temp/"));
            float novo_min = valor / 100.0f;
            if (novo_min >= 22.0f && novo_min <= 25.0f) {
                temp_min = novo_min;
            }

        const char *msg = "Temp. mínima atualizada";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(msg), msg);
    }
        else if (strstr(req, "GET /maximun/temp/"))
        {
            int valor = atoi(strstr(req, "/maximun/temp/") + strlen("/maximun/temp/"));
            float novo_max = valor / 100.0f;
            if (novo_max >= 26.0f && novo_max <= 28.0f) {
                temp_max = novo_max; 
            }



        const char *msg = "Temp. máxima atualizada";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(msg), msg);
    }
        if (strstr(req, "GET /minimun/umidade/"))
        {
            int valor = atoi(strstr(req, "/minimun/umidade/") + strlen("/minimun/umidade/"));
            float novo_min = (float)valor;
            // Verifica se está no intervalo 40 a 50%
            if (novo_min >= 40.0f && novo_min <= 50.0f)
            {
                humi_min = novo_min;
            }
            const char *msg = "Umidade mínima atualizada";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            (int)strlen(msg), msg);
        }
        else if (strstr(req, "GET /maximun/umidade/"))
        {
            int valor = atoi(strstr(req, "/maximun/umidade/") + strlen("/maximun/umidade/"));
            float novo_max = (float)valor;
            if (novo_max >= 60.0f && novo_max <= 70.0f)
            {
                humi_max = novo_max;
            }
            const char *msg = "Umidade máxima atualizada";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            (int)strlen(msg), msg);
        }
        else if (strstr(req, "GET /minimun/pressao/"))
        {
            int valor = atoi(strstr(req, "/minimun/pressao/") + strlen("/minimun/pressao/"));
            float novo_min = (float)valor;
            if (novo_min >= 90.0f && novo_min <= 100.0f)
            {
                press_min = novo_min;
            }
            const char *msg = "Pressão mínima atualizada";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            (int)strlen(msg), msg);
        }
        else if (strstr(req, "GET /maximun/pressao/"))
        {
            int valor = atoi(strstr(req, "/maximun/pressao/") + strlen("/maximun/pressao/"));
            float novo_max = (float)valor;
            if (novo_max >= 110.0f && novo_max <= 120.0f)
            {
                press_max = novo_max;
            }
            const char *msg = "Pressão máxima atualizada";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            (int)strlen(msg), msg);
        }
        
else if (strstr(req, "GET /clima"))
{
    char json_payload[256];
    int json_len = snprintf(json_payload, sizeof(json_payload),
                            "{"
                                "\"sensor\":%d,"
                                "\"tempAHT\":%.2f,"
                                "\"umidade\":%.2f,"
                                "\"tempBMP\":%.2f,"
                                "\"pressao\":%.2f,"
                            "}",
                            bmp280_ativo, data.temperature, data.humidity, temperature, pressure);

    printf("[DEBUG] JSON: %s\n", json_payload);

    hs->len = snprintf(hs->response, sizeof(hs->response),
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       json_len, json_payload);
}
    else
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

// Função principal
int main() {
    stdio_init_all();

    inicializar_perif(); 

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    char str_tmp1[5];  // Buffer para armazenar a string
    char str_press[5];  // Buffer para armazenar a string  
    char str_tmp2[5];  // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string     
    
    // Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    bool cor = true;
    start_http_server();

    while (true)
    {
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo

        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        // Leitura do BMP280
        if(temperature > 1000){
            pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
            altitude = calculate_altitude(pressure);
            bmp280_ativo = true;
            printf("Temperatura BMP: %.2f C\n", temperature/100.0);
            printf("Pressao = %.3f kPa\n", pressure / 1000.0);
            printf("Altitude estimada: %.2f m\n", altitude);
        }
        
        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            bmp280_ativo = false;
        }

        // Transformação dos inteiros em strings para o display
        sprintf(str_tmp1, "%.1fC", temperature / 100.0);  // Converte o inteiro em string
        sprintf(str_press, "%.1fkPa", pressure);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string        
    
        //Acionamento de periféricos em caso do BMP280 estar sendo usado
        if(bmp280_ativo){
            if(pressure > press_max || temperature > temp_max){
                desenhos(desenho1, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(3000, 1.0);
                gpio_put(LED_VERDE, 0);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(pressure > press_max-2.0 || temperature > temp_max-0.8){
                desenhos(desenho2, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(1500, 0.5);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(pressure < press_min+2.0 || temperature < temp_min+0.8){
                desenhos(desenho2, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(1500, 0.5);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(pressure < press_min || temperature < temp_min){
                desenhos(desenho1, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(3000, 1.0);
                gpio_put(LED_VERDE, 0);
                gpio_put(LED_VERMELHO, 1);
            }
            else { 
                desenhos(desenho3, valor_led, pio, sm, 0.0, 0.0, 0.5);
                tocar_buzzer(1000, 0.2);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 0);
        }
            //  Atualiza o conteúdo do display com animações
            ssd1306_fill(&ssd, !cor);                           // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
            ssd1306_line(&ssd, 3, 32, 123, 32, cor);            // Desenha uma linha
            ssd1306_draw_string(&ssd, "Sensor BMP280", 12, 8);  // Desenha uma string
            ssd1306_draw_string(&ssd, "Wi-Fi ativo", 20, 20);   // Desenha uma string
            ssd1306_line(&ssd, 63, 33, 63, 60, cor);            // Desenha uma linha vertical
            ssd1306_draw_string(&ssd, "Temp", 18, 36);  // Desenha uma string
            ssd1306_draw_string(&ssd, "Press", 74, 36);  // Desenha uma string
            ssd1306_draw_string(&ssd, str_tmp1, 14, 46);         // Desenha uma string
            ssd1306_draw_string(&ssd, str_press, 74, 46);         // Desenha uma string
            ssd1306_send_data(&ssd);                            // Atualiza o display
        } 
        
        // Acionamento de periféricos em caso do AHT20 estar sendo usado
        else if (!bmp280_ativo){
            if(data.temperature > temp_max || data.humidity > humi_max){
                desenhos(desenho1, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(3000, 1.0);
                gpio_put(LED_VERDE, 0);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(data.temperature > temp_max-0.8 || data.humidity > humi_max-5.0){
                desenhos(desenho2, valor_led, pio, sm, 0.0, 0.5, 0.5);
                tocar_buzzer(1500, 0.5);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(data.temperature < temp_min+0.8 || data.humidity < humi_min+5.0){
                desenhos(desenho2, valor_led, pio, sm, 0.0, 0.5, 0.5);
                tocar_buzzer(1500, 0.5);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 1);
            } 
            else if(data.temperature < temp_min || data.humidity < humi_min){
                desenhos(desenho1, valor_led, pio, sm, 0.0, 0.5, 0.0);
                tocar_buzzer(3000, 1.0);
                gpio_put(LED_VERDE, 0);
                gpio_put(LED_VERMELHO, 1);
            }
            else { 
                desenhos(desenho3, valor_led, pio, sm, 0.0, 0.0, 0.5);
                tocar_buzzer(1000, 0.2);
                gpio_put(LED_VERDE, 1);
                gpio_put(LED_VERMELHO, 0);
            }
            //  Atualiza o conteúdo do display com animações
            ssd1306_fill(&ssd, !cor);                           // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
            ssd1306_line(&ssd, 3, 32, 123, 32, cor);            // Desenha uma linha
            ssd1306_draw_string(&ssd, "Sensor AHT20", 16, 8);  // Desenha uma string
            ssd1306_draw_string(&ssd, "Wi-Fi ativo", 20, 20);   // Desenha uma string
            ssd1306_line(&ssd, 63, 33, 63, 60, cor);            // Desenha uma linha vertical
            ssd1306_draw_string(&ssd, "Temp", 18, 36);  // Desenha uma string
            ssd1306_draw_string(&ssd, "Umid", 78, 36);  // Desenha uma string
            ssd1306_draw_string(&ssd, str_tmp2, 14, 46);         // Desenha uma string
            ssd1306_draw_string(&ssd, str_umi, 74, 46);         // Desenha uma string
            ssd1306_send_data(&ssd);  
        }
        sleep_ms(1000);
    }
    
    cyw43_arch_deinit();

    return 0;
}