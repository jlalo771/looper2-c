#include <stdio.h>
#include <string.h>

/* SAMPLE_RATE : nombre de samples par seconde (freq echantillonnage standard audio CD)
   MAX_LOOP_SEC : durée maximale d'une boucle en secondes
   MAX_SAMPLES : taille totale du buffer en nombre de samples
   Un float = 4 octets, donc MAX_SAMPLES * 4 = taille en mémoire */
#define SAMPLE_RATE     44100
#define MAX_LOOP_SEC    30
/* #define MAX_SAMPLES     (SAMPLE_RATE * MAX_LOOP_SEC) */
#define MAX_SAMPLES 1024

/* Structure buffer circulaire 
   buffer     : le tableau qui stocke les samples
   write_pos  : position où le prochain sample est écrit
   read_pos   : position où le prochain sample est lu
   loop_length: longueur de la boucle fixée à la fin de l'enregistrement */
typedef struct {
    float buffer[MAX_SAMPLES];
    int   write_pos;
    int   read_pos;
    int   loop_length;
} CircularBuffer;

/* memset met tous les octets du buffer à 0 (silence audio en gros) */
void buffer_init(CircularBuffer* b){
    memset(b->buffer, 0, sizeof(b->buffer));
    b->write_pos   = 0;
    b->read_pos    = 0;
    b->loop_length = 0;
}

/* Écriture d'un sample
   Sample écrit à write_pos, puis j'avance le pointeur.
   Le % MAX_SAMPLES fait boucler le pointeur à 0 quand il atteint la fin du tableau. */
void buffer_write(CircularBuffer* b, float sample){
    b->buffer[b->write_pos] = sample;
    b->write_pos = (b->write_pos + 1) % MAX_SAMPLES;
}

/* Lecture d'un sample
   Lecture à read_pos, puis avancement du pointeur. Le % loop_length fait boucler dans la zone enregistrée uniquement, pas dans tout le tableau. */
float buffer_read(CircularBuffer* b){
    float sample = b->buffer[b->read_pos];
    b->read_pos = (b->read_pos + 1) % b->loop_length;
    return sample;
}

/* Test buffer
   Simulation d'un enregistrement de 8 samples puis lecture de la boucle 2 fois pour vérifier. */
int main(void){
    CircularBuffer buf;
    buffer_init(&buf);

    /* Simulation d'un enregistrement de 8 samples */
    printf("Enregistrement...\n");
    for (int i = 0; i < 8; i++) {
        float sample = (float)i * 0.1f; /* valeurs 0.0, 0.1, 0.2... */
        buffer_write(&buf, sample);
        printf("Ecrit : %.1f a la position %d\n", sample, buf.write_pos - 1);
    }

    /* Fixe la longueur de boucle et remet read_pos a 0 */
    buf.loop_length = buf.write_pos;
    buf.read_pos    = 0;
    printf("\nLongueur de boucle fixee : %d samples\n", buf.loop_length);

    /* Lecture de la boucle 2 fois */
    printf("\nLecture (2 tours)\n");
    for (int tour = 0; tour < 2; tour++) {
        printf("Tour %d : ", tour + 1);
        for (int i = 0; i < buf.loop_length; i++) {
            printf("%.1f ", buffer_read(&buf));
        }
        printf("\n");
    }
    
    return 0;
}