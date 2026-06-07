#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE     44100
#define MAX_LOOP_SEC    30
#define MAX_SAMPLES     (SAMPLE_RATE * MAX_LOOP_SEC)

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING,
    STATE_OVERDUBBING /* Ajout */
} LooperState;

typedef struct {
    float       buffer[MAX_SAMPLES];
    int         write_pos;
    int         read_pos;
    int         loop_length;
    LooperState state;
} LoopTrack;

static LoopTrack g_track;

void track_init(LoopTrack* t)
{
    memset(t->buffer, 0, sizeof(t->buffer));
    t->write_pos   = 0;
    t->read_pos    = 0;
    t->loop_length = 0;
    t->state       = STATE_IDLE;
}

float track_process(LoopTrack* t, float input)
{
    float output = 0.0f;

    switch (t->state) {

        case STATE_IDLE:
            output = input;
            break;

        case STATE_RECORDING:
            t->buffer[t->write_pos] = input;
            t->write_pos++;
            if (t->write_pos >= MAX_SAMPLES)
                t->write_pos = MAX_SAMPLES - 1;
            output = input;
            break;

        case STATE_PLAYING:
            output = t->buffer[t->read_pos];
            t->read_pos = (t->read_pos + 1) % t->loop_length;
            break;

        case STATE_OVERDUBBING:
            /* On additionne le signal entrant au sample existant */
            t->buffer[t->read_pos] += input;
            /* Hard clipping : on force dans [-1.0, 1.0] pour eviter la saturation numerique si on overdub trop */
            if (t->buffer[t->read_pos] >  1.0f) t->buffer[t->read_pos] =  1.0f;
            if (t->buffer[t->read_pos] < -1.0f) t->buffer[t->read_pos] = -1.0f;
            output = t->buffer[t->read_pos];
            /* Les deux pointeurs avancent ensemble */
            t->read_pos = (t->read_pos + 1) % t->loop_length;
            break;
    }

    return output;
}

void pedal_press(LoopTrack* t)
{
    switch (t->state) {

        case STATE_IDLE:
            t->write_pos = 0;
            t->state     = STATE_RECORDING;
            printf("Enregistrement...\n");
            break;

        case STATE_RECORDING:
            t->loop_length = t->write_pos;
            t->read_pos    = 0;
            t->state       = STATE_PLAYING;
            printf("Lecture (%.2f sec)\n",
                   (float)t->loop_length / SAMPLE_RATE);
            break;

        case STATE_PLAYING:
            /* On reste a la meme position dans la boucle
               pour ne pas sauter lors du passage en overdub */
            t->state = STATE_OVERDUBBING;
            printf("Overdubbing...\n");
            break;

        case STATE_OVERDUBBING:
            /* Retour en lecture simple */
            t->state = STATE_PLAYING;
            printf("Lecture...\n");
            break;
    }
}

void audio_callback(ma_device* pDevice, void* pOutput,
                    const void* pInput, ma_uint32 frameCount)
{
    float*       out = (float*)pOutput;
    const float* in  = (const float*)pInput;
    (void)pDevice;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        out[i] = track_process(&g_track, in[i]);
    }
}

int main(void)
{
    track_init(&g_track);

    ma_device_config config  = ma_device_config_init(ma_device_type_duplex);
    config.capture.format    = ma_format_f32;
    config.capture.channels  = 1;
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate        = 44100;
    config.dataCallback      = audio_callback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("Erreur : impossible d'ouvrir le device audio\n");
        return -1;
    }

    ma_device_start(&device);
    printf("LOOPER\n");
    printf("Entree = pedale | Ctrl+C = quitter\n\n");
    printf("Pret.\n");

    while (1) {
        getchar();
        pedal_press(&g_track);
    }

    ma_device_uninit(&device);
    return 0;
}