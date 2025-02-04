#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306_font.h"
#include "display_config.c"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "hardware/pwm.h"

#define DEADZONE 200    // Define a zona morta do joystick
#define THRESHOLD 1000  // Define o limite de movimento do joystick
#define BUTTON_A 5      // Define o pino do botão A
#define BUTTON_B 6      // Define o pino do botão B
#define MAX_PADRAO 6    // Define o tamanho maximo do padrão
#define LED_VERDE 11    // Define o pino do led verde
#define LED_VERMELHO 13 // Define o pino do led vermelho

#define BUZZER_PIN 21 // Configuração do pino do buzzer
#define BUZZER_FREQUENCY 100 // Configuração da frequência do buzzer (em Hz)

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Definição de uma função para emitir um beep com duração especificada
void beep(uint pin, uint duration_ms)
{
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}

// Enumeração para os estados do joystick
typedef enum
{
    CENTRO,
    CIMA,
    BAIXO,
    ESQUERDA,
    DIREITA
} JoystickEstado;

//Array de strings para representar os estados do joystick
const char *estado_str[] = {
    "CENTRO", "CIMA", "BAIXO", "ESQUERDA", "DIREITA"};

//Função para configurar os leds
void set_leds(bool verde, bool vermelho)
{
    gpio_put(LED_VERDE, verde);
    gpio_put(LED_VERMELHO, vermelho);
}

// Função para verificar se a tentativa é igual ao padrão definido
bool verificar_padrao(JoystickEstado *padrao, JoystickEstado *tentativa, int tamanho)
{
    for (int i = 0; i < tamanho; i++)
    {
        if (padrao[i] != tentativa[i])
        {
            return false;
        }
    }
    return true;
}

// Função para definir os estados do joystick de acordo com a leitura do ADC
bool estados_joystick(JoystickEstado *estado_atual, bool *retornou_ao_centro)
{
    adc_select_input(0);
    uint adc_y_raw = adc_read();
    adc_select_input(1);
    uint adc_x_raw = adc_read();

    const uint adc_center = (1 << 11);
    int delta_x = adc_x_raw - adc_center;
    int delta_y = adc_y_raw - adc_center;

    if (delta_x > THRESHOLD)
    {
        *estado_atual = DIREITA;
    }
    else if (delta_x < -THRESHOLD)
    {
        *estado_atual = ESQUERDA;
    }
    else if (delta_y > THRESHOLD)
    {
        *estado_atual = CIMA;
    }
    else if (delta_y < -THRESHOLD)
    {
        *estado_atual = BAIXO;
    }
    else
    {
        *estado_atual = CENTRO;
        *retornou_ao_centro = true;
        return false;
    }

    if (*retornou_ao_centro)
    {
        *retornou_ao_centro = false;
        return true;
    }

    return false;
}

// Função para imprimir a direção do movimento do joystick
void direcao_movimento_joystick(JoystickEstado *estado_atual)
{

    switch (*estado_atual)
    {
    case DIREITA:
        printf("Registrado: Direita\n");
        break;
    case ESQUERDA:
        printf("Registrado: Esquerda\n");
        break;
    case CIMA:
        printf("Registrado: Cima\n");
        break;
    case BAIXO:
        printf("Registrado: Baixo\n");
        break;
    default:
        break;
    }
}

int main()
{
    stdio_init_all(); // Inicializa o terminal serial
    adc_init(); // Inicializa o ADC

    // Configuração do GPIO para o buzzer como saída
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    // Inicializar o PWM no pino do buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Inicializa os botões A e B
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Inicializa os leds verde e vermelho
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    // Inicializa os pinos ADC do joystick
    adc_gpio_init(26);
    adc_gpio_init(27);

    // Inicializa o I2C para o SSD1306
    i2c_init(i2c_default, SSD1306_I2C_CLK * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    SSD1306_init();

    JoystickEstado estado_atual = CENTRO; // Estado atual do joystick
    JoystickEstado padrao[MAX_PADRAO];    // Array para armazenar o padrão definido
    JoystickEstado tentativa[MAX_PADRAO]; // Array para armazenar a tentativa

    bool retornou_ao_centro = true; // Flag para verificar se o joystick retornou ao centro
    bool padrao_definido = false;   // Flag para verificar se o padrão foi definido
    bool bloqueado = false;         // Flag para verificar se o acesso foi bloqueado
    bool autenticado = false;       // Flag para verificar se o acesso foi autenticado
    int contTentativas = 0;         // Contador de tentativas
    int contBloqueio = 0;           // Contador de bloqueios
    int tempo_bloqueio = 2000;      // Tempo de bloqueio em 2000 milissegundos = 2 segundos
    char mensagem[50];              // Defina um tamanho adequado para armazenar a mensagem

    struct render_area area = {
        .start_col = 0,
        .end_col = SSD1306_WIDTH - 1,
        .start_page = 0,
        .end_page = SSD1306_NUM_PAGES - 1};

    calc_render_area_buflen(&area);
    uint8_t buf[area.buflen];
    memset(buf, 0, sizeof(buf));

    // Exibe no display "Controle de Acesso"
    WriteString(buf, calcular_centro("Controle"), 8, "Controle");
    WriteString(buf, calcular_centro("De Acesso"), 32, "De Acesso");
    render(buf, &area);
    sleep_ms(3000);

    // Exibe no display "Pressione A para Iniciar"
    WriteString(buf, calcular_centro("Pressione A"), 8, "Pressione A");
    WriteString(buf, calcular_centro("Para Iniciar"), 32, "Para Iniciar");
    render(buf, &area);
    sleep_ms(1000);

    while (1)
    {
        // Verifica se o botão A foi pressionado E se o padrão ainda nao foi definido
        if (gpio_get(BUTTON_A) == 0 && !padrao_definido)
        {
            // Exibe no display "Defina o padrao de 6 movimentos"
            memset(buf, 0, sizeof(buf));
            WriteString(buf, calcular_centro("Defina o padrao"), 8, "Defina o padrao");
            WriteString(buf, calcular_centro("de 6 movimentos"), 32, "de 6 movimentos");
            render(buf, &area);

            int indice_padrao = 0; // Reseta o indice do padrão

            // Loop para definição do padrão
            while (indice_padrao < MAX_PADRAO)
            {
                if (estados_joystick(&estado_atual, &retornou_ao_centro)) // Verifica o estado do joystick
                {
                    padrao[indice_padrao] = estado_atual; // Armazena o estado atual no padrão
                    indice_padrao++;                      // Incrementa o indice do padrão

                    snprintf(mensagem, sizeof(mensagem), "Movimento %d", indice_padrao); 

                    // Exibe no display o estado atual do joystick
                    direcao_movimento_joystick(&estado_atual);

                    memset(buf, 0, sizeof(buf));
                    WriteString(buf, calcular_centro(mensagem), 8, mensagem);
                    const char *direcao_atual = estado_str[(int)estado_atual];
                    WriteString(buf, calcular_centro(direcao_atual), 32, direcao_atual);
                    render(buf, &area);

                    sleep_ms(400); // Pausa de 400ms
                }
            }
            padrao_definido = true; // Considera que o padrão foi definido

            // Exibe no display que o padrão foi salvo
            memset(buf, 0, sizeof(buf));
            WriteString(buf, calcular_centro("Padrao Salvo"), 8, "Padrao Salvo");
            WriteString(buf, calcular_centro("Pressione B"), 32, "Pressione B");
            render(buf, &area);
        }

        // Verifica se o botão B foi pressionado
        // E o usuário ainda não está autenticado
        // E o sistema não está bloqueado
        // E o padrão já foi definido
        if (gpio_get(BUTTON_B) == 0 && !autenticado && !bloqueado && padrao_definido)
        {
            // Exibe no display a mensagem de autenticação
            memset(buf, 0, sizeof(buf));
            WriteString(buf, calcular_centro("Autenticar"), 8, "Autenticar");
            WriteString(buf, calcular_centro("Mova o Joystick"), 32, "Mova o Joystick");
            render(buf, &area);

            int indice_tentativa = 0; // Reseta o indice da tentativa

            // Loop para autenticação
            while (indice_tentativa < MAX_PADRAO)
            {
                if (estados_joystick(&estado_atual, &retornou_ao_centro)) // Verifica o estado do joystick
                {
                    tentativa[indice_tentativa] = estado_atual; // Armazena o estado atual na tentativa
                    indice_tentativa++;                         // Incrementa o indice da tentativa

                    // Exibe no display o estado atual do joystick
                    direcao_movimento_joystick(&estado_atual);

                    memset(buf, 0, sizeof(buf));
                    WriteString(buf, calcular_centro("Autenticar"), 8, "Autenticar");
                    const char *direcao_atual = estado_str[(int)estado_atual];
                    WriteString(buf, calcular_centro(direcao_atual), 32, direcao_atual);
                    render(buf, &area);

                    sleep_ms(400); // Pausa de 400ms
                }
            }

            // Verifica se a tentativa é igual ao padrao definido
            if (verificar_padrao(padrao, tentativa, MAX_PADRAO))
            {
                autenticado = true; // Considera que o usuário foi autenticado
                contTentativas = 0; // Reseta o contador de tentativas, caso o usuário seja autenticado

                // Exibe no display a mensagem de autorização
                memset(buf, 0, sizeof(buf));
                WriteString(buf, calcular_centro("Acesso"), 8, "Acesso");
                WriteString(buf, calcular_centro("Autorizado"), 32, "Autorizado");
                render(buf, &area);

                // Ativa o led verde
                set_leds(true, false);
                beep(BUZZER_PIN, 2000); // Emite um beep de 2segundos
                sleep_ms(2000);
                set_leds(false, false);
            }
            else
            {
                contTentativas++; // Incrementa o contador de tentativas erradas
                snprintf(mensagem, sizeof(mensagem), "Tentativas: %d", contTentativas); // Exibe o numero de tentativas

                // Se o usuário não foi autenticado, exibe a mensagem de erro no display
                // Exibe no display a mensagem de bloqueio
                memset(buf, 0, sizeof(buf));
                WriteString(buf, calcular_centro("Negado"), 0, "Negado");
                WriteString(buf, calcular_centro(mensagem), 16, mensagem); // Exibe o numero de tentativas
                WriteString(buf, calcular_centro("Pressione B"), 32, "Pressione B");
                render(buf, &area);

                set_leds(false, true); // Acende o led vermelho
                beep(BUZZER_PIN, 2000); // Emite um beep de 2segundos
                sleep_ms(2000); // Pausa de 2 segundos
                set_leds(false, false); // Apaga o led vermelho
            }

            // Verifica se o usuário atingiu o número máximo de tentativas erradas
            if (contTentativas == 3)
            {
                bloqueado = true; // Bloqueia os comandos do sistema

                // Multiplica o tempo de bloqueio por 4 toda vez que o sistema for bloqueado
                if (contBloqueio++)
                {
                    tempo_bloqueio *= 4;
                }

                snprintf(mensagem, sizeof(mensagem), "%d segundos", tempo_bloqueio / 1000); // Exibe o tempo de bloqueio

                // Exibe no display a mensagem de bloqueio
                memset(buf, 0, sizeof(buf));
                WriteString(buf, calcular_centro("Aguarde"), 0, "Aguarde");
                WriteString(buf, calcular_centro("Bloqueado por"), 16, "Bloqueado por");
                WriteString(buf, calcular_centro(mensagem), 32, mensagem);
                render(buf, &area);

                // Acende o led vermelho
                set_leds(false, true);
                sleep_ms(tempo_bloqueio);
                set_leds(false, false);

                // Mensagem informando que o sistema foi desbloqueado e o usuário pode tentar novamente pressionando a tecla B
                memset(buf, 0, sizeof(buf));
                WriteString(buf, calcular_centro("Comandos"), 0, "Comandos");
                WriteString(buf, calcular_centro("Desbloqueados"), 16, "Desbloqueados");
                WriteString(buf, calcular_centro("Pressione B"), 32, "Pressione B");
                render(buf, &area);

                contTentativas = 0; // Reinicia o contador de tentativas após o desbloqueio
                bloqueado = false;  // Desbloqueia o sistema para operação
            }
        }

        sleep_ms(100); // Pausa de 100ms
    }

    return 0;
}