#define EIDSP_QUANTIZE_FILTERBANK   0

static char lastLabel[32] = "Unknown";

const char* getLastVoiceLabel() {
  return lastLabel;
}

#include <PDM.h>
#include <VoiceMyRose_inferencing.h>

typedef struct {
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false;

void setupVoice()
{
    ei_printf("[VOICE MODULE] INITIALIZING VOICE MODULE...\n");
    if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }
    ei_printf("[VOICE MODULE] VOICE MODULE INICIALIZED\n");
}

void loopVoice()
{
    ei_printf("[VOICE MODULE] STARTING VOICE INFERENCE IN 2 SECONDS\n");
    delay(2000);
    ei_printf("[VOICE MODULE] RECORDING...\n");

    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    ei_printf("[VOICE MODULE] RECORDED!\n");

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    float best = 0.0f;
    size_t idx = 0;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
        if (result.classification[i].value > best) {
            best = result.classification[i].value;
            idx = i;
        }
    }
    strncpy(lastLabel, result.classification[idx].label, sizeof(lastLabel)-1);
    lastLabel[sizeof(lastLabel)-1] = '\0';
    ei_printf("[VOICE MODULE] INFERENCE RESULT: %s (%.2f%%)\n", result.classification[idx].label, best * 100.0f);
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
}

static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (inference.buf_ready == 0) {
        for(int i = 0; i < bytesRead>>1; i++) {
            inference.buffer[inference.buf_count++] = sampleBuffer[i];

            if(inference.buf_count >= inference.n_samples) {
                inference.buf_count = 0;
                inference.buf_ready = 1;
                break;
            }
        }
    }
}

static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if(inference.buffer == NULL) {
        return false;
    }

    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize(4096);

    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
        microphone_inference_end();

        return false;
    }

    PDM.setGain(127);

    return true;
}

static bool microphone_inference_record(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;

    while(inference.buf_ready == 0) {
        delay(10);
    }

    return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

    return 0;
}

static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif