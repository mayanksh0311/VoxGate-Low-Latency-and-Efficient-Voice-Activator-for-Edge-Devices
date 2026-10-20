#include <VoxGateV2_inferencing.h>
#include <driver/i2s.h>


#define I2S_SD      4     
#define I2S_WS      5     
#define I2S_SCK     6     
#define I2S_PORT    I2S_NUM_0

#define LED_PIN     2     


#define SAMPLE_RATE 16000
#define BUFFER_SIZE 512


#define VAD_RMS_THRESHOLD      350.0f  
#define CONFIDENCE_THRESHOLD   0.85f   
#define K_OF_N_K               3      
#define K_OF_N_N               5       
#define REFRACTORY_LOCKOUT_MS  2500    

typedef struct {
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static bool record_status = true;


static uint8_t hit_history[K_OF_N_N] = {0};
static uint8_t history_idx = 0;
static unsigned long last_trigger_time = 0;


static esp_err_t i2s_init(void) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 standard
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    esp_err_t ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (ret != ESP_OK) return ret;

    return i2s_set_pin(I2S_PORT, &pin_config);
}

static float calculate_rms(int16_t *samples, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += (float)samples[i] * (float)samples[i];
    }
    return sqrtf(sum / (float)count);
}


static void capture_samples(void* arg) {
    size_t bytes_read = 0;
    int32_t i2s_raw_buffer[BUFFER_SIZE];

    while (record_status) {
        i2s_read(I2S_PORT, (void*)i2s_raw_buffer, sizeof(i2s_raw_buffer), &bytes_read, portMAX_DELAY);

        if (bytes_read > 0) {
            size_t samples_read = bytes_read / sizeof(int32_t);
            int16_t converted_samples[BUFFER_SIZE];

            for (size_t i = 0; i < samples_read; i++) {
                
                converted_samples[i] = (int16_t)(i2s_raw_buffer[i] >> 14);
            }

           
            for (size_t i = 0; i < samples_read; i++) {
                inference.buffer[inference.buf_count++] = converted_samples[i];

                if (inference.buf_count >= inference.n_samples) {
                    inference.buf_ready = 1;
                    inference.buf_count = 0;
                }
            }
        }
    }
    vTaskDelete(NULL);
}

static int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}


void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("=========================================");
    Serial.println("  VoxGate v2 - Low-Latency Edge Engine   ");
    Serial.println("=========================================");

    if (i2s_init() != ESP_OK) {
        Serial.println("[-] Failed to initialize I2S interface!");
        while (1);
    }
    Serial.println("[+] I2S INMP441 Microphone Driver Initialized");


    inference.n_samples = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    inference.buffer = (int16_t *)malloc(inference.n_samples * sizeof(int16_t));
    if (!inference.buffer) {
        Serial.println("[-] Failed to allocate inferencing memory buffer!");
        while (1);
    }
    inference.buf_count = 0;
    inference.buf_ready = 0;

  
    xTaskCreatePinnedToCore(capture_samples, "CaptureTask", 1024 * 4, NULL, 10, NULL, 0);

    Serial.printf("[+] Model Ready. Sampling frequency: %d Hz\n", EI_CLASSIFIER_FREQUENCY);
    Serial.println("[+] Listening for Wake-Word...");
}

void loop() {
    if (inference.buf_ready == 1) {
        inference.buf_ready = 0;

     
        float rms = calculate_rms(inference.buffer, inference.n_samples);
        if (rms < VAD_RMS_THRESHOLD) {
            
            return;
        }

       
        signal_t signal;
        signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
        signal.get_data = &raw_feature_get_data;
        ei_impulse_result_t result = { 0 };

      
        EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);
        if (r != EI_IMPULSE_OK) {
            Serial.printf("[-] Inference failed (%d)\n", r);
            return;
        }

     
        float keyword_score = 0.0f;
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            if (strcmp(result.classification[ix].label, "keyword") == 0) {
                keyword_score = result.classification[ix].value;
                break;
            }
        }

      
        hit_history[history_idx] = (keyword_score >= CONFIDENCE_THRESHOLD) ? 1 : 0;
        history_idx = (history_idx + 1) % K_OF_N_N;

        uint8_t hit_count = 0;
        for (uint8_t i = 0; i < K_OF_N_N; i++) {
            hit_count += hit_history[i];
        }

       
        unsigned long current_time = millis();
        if (hit_count >= K_OF_N_K && (current_time - last_trigger_time > REFRACTORY_LOCKOUT_MS)) {
            last_trigger_time = current_time;

            Serial.println("\n*****************************************");
            Serial.printf(">>> [VOXGATE TRIGGERED] Keyword Conf: %.2f%% <<<\n", keyword_score * 100.0f);
            Serial.printf(">>> Latency: %d ms | DSP: %d ms <<<\n", result.timing.classification, result.timing.dsp);
            Serial.println("*****************************************\n");

        
            digitalWrite(LED_PIN, HIGH);
            delay(150);
            digitalWrite(LED_PIN, LOW);

            memset(hit_history, 0, sizeof(hit_history));
        }
    }
}