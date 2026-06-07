#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE    44100
#define MAX_LOOP_SEC   30
#define MAX_SAMPLES    (SAMPLE_RATE * MAX_LOOP_SEC)
#define NUM_TRACKS     2

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING,
    STATE_OVERDUBBING
} LooperState;

typedef struct {
    float       buffer[MAX_SAMPLES];
    int         write_pos;
    int         read_pos;
    int         loop_length;
    LooperState state;
} LoopTrack;

/* Je regroupe les deux pistes et les informations partagees
   dans une seule structure.
   master_loop_length : fixe par la track 1, impose aux suivantes
   active_track       : indice de la piste actuellement pilotee */
typedef struct {
    LoopTrack tracks[NUM_TRACKS];
    int       master_loop_length;
    int       active_track;
} LooperContext;

static LooperContext g_ctx;

void context_init(LooperContext* ctx)
{
    for (int i = 0; i < NUM_TRACKS; i++) {
        memset(ctx->tracks[i].buffer, 0, sizeof(ctx->tracks[i].buffer));
        ctx->tracks[i].write_pos   = 0;
        ctx->tracks[i].read_pos    = 0;
        ctx->tracks[i].loop_length = 0;
        ctx->tracks[i].state       = STATE_IDLE;
    }
    ctx->master_loop_length = 0;
    ctx->active_track       = 0;
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
            /* Arret automatique quand on atteint loop_length :
               pour la track 1, loop_length = MAX_SAMPLES (pas de limite)
               pour la track 2, loop_length = master_loop_length (impose) */
            if (t->write_pos >= t->loop_length) {
                t->read_pos = 0;
                t->state    = STATE_PLAYING;
                printf("Track auto-bouclee\n");
            }
            output = input;
            break;

        case STATE_PLAYING:
            output = t->buffer[t->read_pos];
            t->read_pos = (t->read_pos + 1) % t->loop_length;
            break;

        case STATE_OVERDUBBING:
            t->buffer[t->read_pos] += input;
            if (t->buffer[t->read_pos] >  1.0f) t->buffer[t->read_pos] =  1.0f;
            if (t->buffer[t->read_pos] < -1.0f) t->buffer[t->read_pos] = -1.0f;
            output = t->buffer[t->read_pos];
            t->read_pos = (t->read_pos + 1) % t->loop_length;
            break;
    }

    return output;
}

void pedal_press(LooperContext* ctx)
{
    LoopTrack* t = &ctx->tracks[ctx->active_track];

    switch (t->state) {

        case STATE_IDLE:
            t->write_pos = 0;
            t->state     = STATE_RECORDING;

            if (ctx->active_track == 0) {
                /* Track 1 : pas de limite connue a l'avance,
                   on laisse MAX_SAMPLES comme garde-fou */
                t->loop_length = MAX_SAMPLES;
                printf("Track 1 : enregistrement...\n");
            } else {
                /* Track 2 : la longueur est imposee par la track 1 */
                if (ctx->master_loop_length == 0) {
                    printf("Erreur : enregistre d'abord la track 1\n");
                    t->state = STATE_IDLE;
                    return;
                }
                t->loop_length = ctx->master_loop_length;
                printf("Track 2 : enregistrement (%.2f sec imposees)...\n",
                       (float)ctx->master_loop_length / SAMPLE_RATE);
            }
            break;

        case STATE_RECORDING:
            if (ctx->active_track == 0) {
                /* 2e appui sur track 1 : on fixe master_loop_length */
                t->loop_length          = t->write_pos;
                ctx->master_loop_length = t->loop_length;
                t->read_pos             = 0;
                t->state                = STATE_PLAYING;
                printf("Track 1 : lecture (%.2f sec)\n",
                       (float)t->loop_length / SAMPLE_RATE);
            }
            /* Pour track 2, l'arret est automatique dans track_process */
            break;

        case STATE_PLAYING:
            t->state = STATE_OVERDUBBING;
            printf("Track %d : overdubbing...\n", ctx->active_track + 1);
            break;

        case STATE_OVERDUBBING:
            t->state = STATE_PLAYING;
            printf("Track %d : lecture...\n", ctx->active_track + 1);
            break;
    }
}

/* Chaque piste est traitee independamment et on mixe les sorties.
   Un clipping global est applique sur le mix final. */
void audio_callback(ma_device* pDevice, void* pOutput,
                    const void* pInput, ma_uint32 frameCount)
{
    float*       out = (float*)pOutput;
    const float* in  = (const float*)pInput;
    (void)pDevice;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        float mix = 0.0f;
        for (int j = 0; j < NUM_TRACKS; j++)
            mix += track_process(&g_ctx.tracks[j], in[i]);
        if (mix >  1.0f) mix =  1.0f;
        if (mix < -1.0f) mix = -1.0f;
        out[i] = mix;
    }
}

int main(void)
{
    context_init(&g_ctx);

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
    printf("LOOPER 2 TRACKS\n");
    printf("Entree = pedale | T = changer de track | Ctrl+C = quitter\n\n");
    printf("Track 1 active. Pret.\n");

    char c;
    while (1) {
        c = getchar();
        if (c == '\n')
            pedal_press(&g_ctx);
        else if (c == 't' || c == 'T') {
            g_ctx.active_track = (g_ctx.active_track + 1) % NUM_TRACKS;
            printf("Track active : %d\n", g_ctx.active_track + 1);
        }
    }

    ma_device_uninit(&device);
    return 0;
}