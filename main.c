/* miniaudio est une bibliothèque type header-only (tout son code est dans un seul fichier .h):
   "define" dit à miniaudio d'inclure son implémentation ici */
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>  /* pour printf et getchar */

/* La fonction de Callback audio 
   Cette fonction est appelée automatiquement par la carte son.
   - pDevice    : informations sur le device audio (non utilisé ici mais c'est imposé par miniaudio)
   - pOutput    : tableau de samples à envoyer vers les haut-parleurs
   - pInput     : tableau de samples venant du microphone
   - frameCount : nombre de samples dans ce bloc (typiquement 256) */
void audio_callback(ma_device* pDevice, void* pOutput,
                    const void* pInput, ma_uint32 frameCount)
{
    /* Je cast les pointeurs void* en float*
       car j'ai choisi le format f32 (floats entre -1.0 et +1.0) */
    float*       out = (float*)pOutput;
    const float* in  = (const float*)pInput;

    /* Parcours de chaque sample du bloc
       et copie de l'entrée vers la sortie :
       je m'entends directement dans le casque */
    for (ma_uint32 i = 0; i < frameCount; i++) {
        out[i] = in[i];
    }
}

int main(void)
{
    /* Je configure l'interface audio de mon pc (device = carte son qui gère micro + haut-parleurs) en mode duplex :
       capture (micro) + playback (haut-parleurs) simultanés */
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);

    config.capture.format    = ma_format_f32;  /* samples en float 32 bits */
    config.capture.channels  = 1;              /* mono (stéréo c'est dur) */
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate        = 44100;          /* fréquence d'échantillonnage standard CD */
    config.dataCallback      = audio_callback; /* APPEL DE MA FONCTION CALLBACK */

    /* Initialisation du device avec cette configuration
       NULL = utilise le device audio par défaut du système */
    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("Erreur : impossible d'ouvrir le device audio\n");
        return -1;
    }

    /* Lance le device : le callback commence à être appelé */
    ma_device_start(&device);
    printf("Audio en cours... Appuyer sur Entree pour quitter\n");

    /* getchar() bloque le programme jusqu'à un appui sur Entrée
       sans ça, main() se termine immédiatement */
    getchar();

    /* Libère les ressources audio */
    ma_device_uninit(&device);
    return 0;
}