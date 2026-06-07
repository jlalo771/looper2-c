#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define SAMPLE_RATE    44100
#define MAX_LOOP_SEC   30
#define MAX_SAMPLES    (SAMPLE_RATE * MAX_LOOP_SEC)
#define NUM_TRACKS     2
#define VU_WIDTH       16

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING,
    STATE_OVERDUBBING
} LooperState;

/* Noms des etats pour l'affichage */
const char* state_name[] = {
    "IDLE      ",
    "RECORDING ",
    "PLAYING   ",
    "OVERDUB   "
};

typedef struct {
    float       buffer[MAX_SAMPLES];
    int         write_pos;
    int         read_pos;
    int         loop_length;
    LooperState state;
} LoopTrack;

typedef struct {
    LoopTrack      tracks[NUM_TRACKS];
    int            master_loop_length;
    int            active_track;
    /* volatile : indique au compilateur que cette valeur
       peut changer a tout moment depuis un autre thread.
       Le compilateur ne la mettra pas en cache. */
    volatile float vu_level;
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
    ctx->vu_level           = 0.0f;
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
            if (t->write_pos >= t->loop_length) {
                t->read_pos = 0;
                t->state    = STATE_PLAYING;
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
                t->loop_length = MAX_SAMPLES;
            } else {
                if (ctx->master_loop_length == 0) return;
                t->loop_length = ctx->master_loop_length;
            }
            break;

        case STATE_RECORDING:
            if (ctx->active_track == 0) {
                t->loop_length          = t->write_pos;
                ctx->master_loop_length = t->loop_length;
                t->read_pos             = 0;
                t->state                = STATE_PLAYING;
            }
            break;

        case STATE_PLAYING:
            t->state = STATE_OVERDUBBING;
            break;

        case STATE_OVERDUBBING:
            t->state = STATE_PLAYING;
            break;
    }
}

/* ── Callback audio ──────────────────────────────────────────────
   Seule nouveaute : calcul du niveau crete pour le VU-metre.
   On cherche le sample le plus fort du bloc (valeur absolue max).
   Pas de printf ici : operation trop lente pour le temps reel. */
void audio_callback(ma_device* pDevice, void* pOutput,
                    const void* pInput, ma_uint32 frameCount)
{
    float*       out  = (float*)pOutput;
    const float* in   = (const float*)pInput;
    float        peak = 0.0f;
    (void)pDevice;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        float mix = 0.0f;
        for (int j = 0; j < NUM_TRACKS; j++)
            mix += track_process(&g_ctx.tracks[j], in[i]);
        if (mix >  1.0f) mix =  1.0f;
        if (mix < -1.0f) mix = -1.0f;
        out[i] = mix;

        /* Valeur absolue du sample entrant */
        float abs_in = in[i] < 0 ? -in[i] : in[i];
        if (abs_in > peak) peak = abs_in;
    }

    /* Ecriture du niveau crete dans la variable partagee */
    g_ctx.vu_level = peak;
}

/* ── Affichage ───────────────────────────────────────────────────
   Dessine une barre ASCII de longueur 'width'.
   'ratio' entre 0.0 et 1.0 determine la proportion remplie. */
void draw_bar(float ratio, int width, char full, char empty)
{
    int filled = (int)(ratio * width);
    if (filled > width) filled = width;
    for (int i = 0; i < width; i++)
        putchar(i < filled ? full : empty);
}

void display_refresh(LooperContext* ctx)
{
    /* Repositionne le curseur en haut sans effacer l'ecran */
    COORD pos = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);

    printf("LOOPER\n\n");

    for (int i = 0; i < NUM_TRACKS; i++) {
        LoopTrack* t      = &ctx->tracks[i];
        /* '>' indique la piste active */
        int        active = (i == ctx->active_track);

        /* Calcul de la progression dans la boucle */
        float progress = 0.0f;
        if (t->loop_length > 0) {
            int pos_in_loop = (t->state == STATE_RECORDING)
                              ? t->write_pos
                              : t->read_pos;
            progress = (float)pos_in_loop / t->loop_length;
        }

        printf("[TRACK %d]%s ", i + 1, active ? ">" : " ");
        draw_bar(progress, VU_WIDTH, '#', '-');
        printf("  %s", state_name[t->state]);

        if (t->loop_length > 0 && t->state != STATE_IDLE)
            printf("  %.1fs", (float)t->loop_length / SAMPLE_RATE);

        printf("\n");
    }

    /* VU-metre : affiche le niveau du signal entrant */
    printf("\nVU-in : [");
    draw_bar(ctx->vu_level, VU_WIDTH, '=', ' ');
    printf("]\n");

    printf("\nEntree=pedale | T=changer track | Ctrl+C=quitter\n");
}

/* Thread d'affichage
   Tourne en parallele du thread audio et du thread principal.
   Sleep(100) : rafraichissement toutes les 100ms.
   On cache le curseur pour un affichage plus propre. */
DWORD WINAPI display_thread(LPVOID param)
{
    (void)param;
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
    system("cls");

    while (1) {
        display_refresh(&g_ctx);
        Sleep(100);
    }
    return 0;
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

    /* Lancement du thread d'affichage.
       Il tourne en parallele sans bloquer le thread principal. */
    CreateThread(NULL, 0, display_thread, NULL, 0, NULL);

    char c;
    while (1) {
        c = getchar();
        if (c == '\n')
            pedal_press(&g_ctx);
        else if (c == 't' || c == 'T')
            g_ctx.active_track = (g_ctx.active_track + 1) % NUM_TRACKS;
    }

    ma_device_uninit(&device);
    return 0;
}