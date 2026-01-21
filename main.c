#include "audio.h"
#include <stdio.h>
#include "wav_table.h"

int main(int argc, char **argv)
{
    if (!audio_init()) return 1;

    if (!play_embedded_wav_by_name("message001"))
        return 1;               /* error already printed */

    audio_cleanup();
    return 0;
}
