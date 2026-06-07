#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE     44100
#define MAX_LOOP_SEC    30
#define MAX_SAMPLES     (SAMPLE_RATE * MAX_LOOP_SEC)

/* enum sert à nommer clairement les etats du looper
   En memoire, IDLE=0, RECORDING=1, PLAYING=2 */
typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING
} LooperState;

/* static : place la structure dans le segment de donnees
   et non sur la pile, pour eviter le stack overflow */
typedef struct {
    float       buffer[MAX_SAMPLES];
    int         write_pos;
    int         read_pos;
    int         loop_length;
    LooperState state;
} LoopTrack;

static LoopTrack g_track;

/* On remet tout a zero : silence et etat IDLE */
void track_init(LoopTrack* t){
    memset(t->buffer, 0, sizeof(t->buffer));
    t->write_pos   = 0;
    t->read_pos    = 0;
    t->loop_length = 0;
    t->state       = STATE_IDLE;
}

/* Traitement d'un sample
   Appelee pour chaque sample dans le callback audio.
   Selon l'etat, on enregistre, on lit, ou on laisse passer. */
float track_process(LoopTrack* t, float input){
    float output = 0.0f;

    switch (t->state) {

        case STATE_IDLE:
            /* Pas d'enregistrement, pas de lecture :
               le signal d'entree passe tel quel */
            output = input;
            break;

        case STATE_RECORDING:
            /* Sample entrant ecrit dans le buffer */
            t->buffer[t->write_pos] = input;
            t->write_pos++;
            /* Securite : bloque si la fin du buffer est atteinte */
            if (t->write_pos >= MAX_SAMPLES)
                t->write_pos = MAX_SAMPLES - 1;
            /* Je m'entends en direct pendant l'enregistrement */
            output = input;
            break;

        case STATE_PLAYING:
            /* Sample lu a read_pos */
            output = t->buffer[t->read_pos];
            /* % loop_length : repart au debut de la boucle quand on arrive a la fin */
            t->read_pos = (t->read_pos + 1) % t->loop_length;
            break;
    }

    return output;
}

/* Appui pedale (transitions entre etats) */
void pedal_press(LoopTrack* t){
    switch (t->state) {

        case STATE_IDLE:
            /* 1er appui : debut de l'enregistrement */
            t->write_pos = 0;
            t->state     = STATE_RECORDING;
            printf("Enregistrement...\n");
            break;

        case STATE_RECORDING:
            /* 2e appui : fin de l'enregistrement
               On fixe loop_length = nombre de samples enregistres
               On remet read_pos a 0 pour relire depuis le debut */
            t->loop_length = t->write_pos;
            t->read_pos    = 0;
            t->state       = STATE_PLAYING;
            printf("Lecture (%.2f sec)\n",
                   (float)t->loop_length / SAMPLE_RATE);
            break;

        case STATE_PLAYING:
            /* 3e appui : retour a l'etat initial */
            track_init(t);
            printf("Remis a zero.\n");
            break;
    }
}

/* Callback audio, appelee ~172 fois par seconde par la carte son.
   track_process appele pour chaque sample. */
void audio_callback(ma_device* pDevice, void* pOutput,
                    const void* pInput, ma_uint32 frameCount){
    float*       out = (float*)pOutput;
    const float* in  = (const float*)pInput;
    (void)pDevice;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        out[i] = track_process(&g_track, in[i]);
    }
}

int main(void){
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
    printf("LOOPER trop cool\n");
    printf("Entree = pedale | Ctrl+C = quitter\n\n");
    printf("Pret.\n");

    /* Boucle principale : chaque appui sur Entree = pedale */
    while (1) {
        getchar();
        pedal_press(&g_track);
    }

    ma_device_uninit(&device);
    return 0;
}