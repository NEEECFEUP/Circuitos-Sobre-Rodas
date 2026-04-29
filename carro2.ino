// __________________ CONSTANTES __________________
const uint8_t ADC_CHANNELS[5] = {1, 2, 3, 4, 5};  // Usando A1 a A5
#define NUM_SENSORS 5
#define LINE_LOST 999
const int weights[NUM_SENSORS] = {-2, -1, 0, 1, 2};
struct Motor { 
    int current;   // Velocidade atual (0-255)
    int target;    // Velocidade desejada (0-255)
};

// Instâncias dos motores
Motor leftMotor = {0, 0};
Motor rightMotor = {0, 0};


// __________________ Exercicio __________________
// Emplementar controlo do carro no setup e loop podendo fazer uso das funções de ajuda

#define IN1 5   // Pino para motor esquerdo (sentido A)
#define IN2 6   // Pino para motor esquerdo (sentido B)
#define IN3 9   // Pino para motor direito (sentido A)
#define IN4 10  // Pino para motor direito (sentido B)

// Threshold para detecção da linha (0-1023)
int threshold = 850;  // Ajustar: valores mais altos = mais sensível

void setup() {
    // Inicializa comunicação serial para debug
    Serial.begin(9600);
    Serial.println("=== SEGUIDOR DE LINHA ARDUINO UNO ===");
    
    // Inicializa hardware
    adcInit();      // Configura ADC para leitura rápida
    motorsInit();   // Configura pinos dos motores
    
    // Para motores inicialmente
    setMotors(0, 0);
    
    delay(1000);  // Pausa inicial
}

void loop() {    
    // 1. Lê a posição da linha
    int error = readLineError();
    
    // 2. Verifica se perdeu a linha
    if (error == LINE_LOST) {
        // Estratégia de recuperação: gira no lugar
        setMotors(80, -80);
        Serial.println("PERDEU LINHA! Buscando...");
    } 
    else {
        // 3. Calcula correção usando controle proporcional
        // KP = 40 (quanto maior, mais rápido corrige)
        int correction = error * 40;
        
        // 4. Limita correção para não sobrecarregar motores
        correction = constrain(correction, -150, 150);
        
        // 5. Aplica correção aos motores
        // Velocidade base = 100, correção adicionada/subtraída
        int leftSpeed = 100 - correction;
        int rightSpeed = 100 + correction;
        
        // 6. Limita velocidades entre -255 e 255
        leftSpeed = constrain(leftSpeed, -255, 255);
        rightSpeed = constrain(rightSpeed, -255, 255);
        
        // 7. Define velocidades dos motores
        setMotors(leftSpeed, rightSpeed);
        
        // Debug (opcional)
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 100) {
            Serial.print("Erro: ");
            Serial.print(error);
            Serial.print(" | Correção: ");
            Serial.print(correction);
            Serial.print(" | Motores: ");
            Serial.print(leftSpeed);
            Serial.print(", ");
            Serial.println(rightSpeed);
            lastDebug = millis();
        }
    }
    
    // 8. Atualiza motores (aplica aceleração suave)
    updateMotors();
    
    delay(10);  // Loop principal a ~100Hz
}

// __________________ FUNÇÕES DE AJUDA __________________
//                        IGNORAR

// Implementa as funções de ajuda para o exercicio

/**
 * Inicializa o ADC do ATmega328P para leitura rápida
 * 
 * Configurações:
 * - Referência: AVcc (5V)
 * - Ajuste à direita (bits mais baixos no ADCL)
 * - Prescaler: 128 (16MHz/128 = 125kHz - dentro da faixa recomendada)
 * 
 * @note Desabilita entradas digitais para reduzir consumo de energia
 */
void adcInit() { 
    // Configura referência como AVcc (5V) e ajuste à direita
    ADMUX = (1 << REFS0);
    
    // Habilita ADC e define prescaler para 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    
    // Desabilita entradas digitais nos pinos ADC para reduzir ruído
    DIDR0 = (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D) | 
            (1 << ADC3D) | (1 << ADC4D) | (1 << ADC5D);
}

/**
 * Leitura rápida do ADC em um canal específico
 * 
 * @param ch Canal ADC (0-5)
 * @return Valor convertido (0-1023)
 * 
 * @note Mais rápido que analogRead() (~13µs vs 100µs)
 * @note Não bloqueante durante a conversão
 */
inline uint16_t adcRead(uint8_t ch) { 
    // Seleciona canal com bits de referência
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
    
    // Inicia conversão
    ADCSRA |= (1 << ADSC);
    
    // busy-wait
    while (ADCSRA & (1 << ADSC));
    
    return ADC;
}

/**
 * Lê os sensores e calcula a posição da linha
 * 
 * @return Posição da linha (-3 a +3) ou LINE_LOST (999)
 * 
 * Cálculo: Soma ponderada dos sensores ativos
 * Exemplo: Sensor esquerdo ativo: -2
 *          Sensor direito ativo: +2
 *          Centro ativo: 0
 * 
 * @note Sensores ativos = valor lido < threshold (linha preta)
 */
int readLineError() { 
    int err = 0;            // Erro acumulado
    bool det = false;       // Flag de detecção
    
    // Varre todos os sensores
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        // Lê sensor e compara com threshold
        if (adcRead(ADC_CHANNELS[i]) < threshold) {
            err += weights[i];  // Adiciona peso do sensor
            det = true;         // Marca que detectou linha
        }
    }
    
    // Retorna erro ou código de linha perdida
    return det ? err : LINE_LOST;
}

/**
 * Inicializa os pinos dos motores
 * 
 * @note Configura pinos como saída e desliga motores
 * @note Importante: Verifique a pinagem da sua ponte H
 */
void motorsInit() { 
    // Configura pinos como saída
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    
    // Garante que motores começam desligados
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}

/**
 * Atualiza o estado de um motor (aceleração suave)
 * 
 * @param m Referência para a estrutura do motor
 * @param inA Pino de sentido A (PWM)
 * @param inB Pino de sentido B (PWM)
 * 
 * @note Implementa rampa de aceleração/desaceleração
 * @note Antes de inverter direção, zera a velocidade atual
 */
void updateMotor(Motor &m, uint8_t inA, uint8_t inB) { 
    // Se mudando de direção, primeiro zera a velocidade
    if ((m.current > 0 && m.target < 0) || (m.current < 0 && m.target > 0)) {
        m.current = 0;
    }
    
    // Incrementa ou decrementa velocidade atual para alcançar o alvo
    if (m.current < m.target) {
        m.current++;  // Acelera
    } 
    else if (m.current > m.target) {
        m.current--;  // Desacelera
    }
    
    // Controle da ponte H (L298N/L293D):
    // - Sentido A: inA=HIGH, inB=LOW
    // - Sentido B: inA=LOW, inB=HIGH
    // - Parado: inA=LOW, inB=LOW
    // - Freio: inA=HIGH, inB=HIGH (não usado aqui)
    
    // Aplica PWM baseado na direção (corrigido)
    if (m.current > 0) {
        // Sentido positivo (para frente)
        analogWrite(inA, abs(m.current));
        analogWrite(inB, 0);
    } 
    else if (m.current < 0) {
        // Sentido negativo (para trás)
        analogWrite(inA, 0);
        analogWrite(inB, abs(m.current));
    } 
    else {
        // Parado
        analogWrite(inA, 0);
        analogWrite(inB, 0);
    }
}

/**
 * Atualiza ambos os motores
 * 
 * @note Chama updateMotor para cada motor
 */
void updateMotors() { 
    updateMotor(leftMotor, IN1, IN2);
    updateMotor(rightMotor, IN3, IN4);
}

/**
 * Define as velocidades alvo dos motores
 * 
 * @param l Velocidade alvo do motor esquerdo (-255 a 255)
 * @param r Velocidade alvo do motor direito (-255 a 255)
 * 
 * @note Valores negativos = movimento para trás
 * @note A velocidade atual será ajustada gradualmente por updateMotors()
 */
void setMotors(int l, int r) { 
    leftMotor.target = l;
    rightMotor.target = r;
}

// ==================== FUNÇÕES ADICIONAIS (ÚTEIS) ====================

/**
 * Função de calibração simples
 * 
 * @note Pressione o botão de reset para calibrar
 * @note Coloque todos os sensores sobre a linha preta
 */
void calibrateThreshold() {
    Serial.println("=== CALIBRAÇÃO ===");
    Serial.println("Coloque sensores sobre linha PRETA");
    delay(3000);
    
    int sum = 0;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        int value = adcRead(ADC_CHANNELS[i]);
        sum += value;
        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(value);
    }
    
    // Calcula threshold como 80% do valor médio na linha preta
    threshold = (sum / NUM_SENSORS) * 0.8;
    
    Serial.print("Novo threshold: ");
    Serial.println(threshold);
    delay(2000);
}

/**
 * Testa todos os sensores individualmente
 * 
 * @note Útil para verificar conexões e funcionamento
 */
void testSensors() {
    Serial.println("=== TESTE DE SENSORES ===");
    
    while (true) {
        for (uint8_t i = 0; i < NUM_SENSORS; i++) {
            int value = adcRead(ADC_CHANNELS[i]);
            bool onLine = value < threshold;
            
            Serial.print("S");
            Serial.print(i);
            Serial.print(": ");
            Serial.print(value);
            Serial.print(onLine ? " (PRETO)" : " (BRANCO)");
            Serial.print(" | ");
        }
        Serial.println();
        delay(250);
    }
}

/**
 * Testa os motores individualmente
 * 
 * @note Verifica se todos os motores e sentidos funcionam
 */
void testMotors() {
    Serial.println("=== TESTE DE MOTORES ===");
    
    // Teste motor esquerdo
    Serial.println("Motor Esquerdo - Frente");
    setMotors(100, 0);
    for (int i = 0; i < 100; i++) {
        updateMotors();
        delay(10);
    }
    
    Serial.println("Motor Esquerdo - Trás");
    setMotors(-100, 0);
    for (int i = 0; i < 100; i++) {
        updateMotors();
        delay(10);
    }
    
    // Teste motor direito
    Serial.println("Motor Direito - Frente");
    setMotors(0, 100);
    for (int i = 0; i < 100; i++) {
        updateMotors();
        delay(10);
    }
    
    Serial.println("Motor Direito - Trás");
    setMotors(0, -100);
    for (int i = 0; i < 100; i++) {
        updateMotors();
        delay(10);
    }
    
    // Para motores
    setMotors(0, 0);
    Serial.println("Teste terminado");
}