#include "config.h"

static const AVInputFormat * const demuxer_list[] = {
    &ff_asf_demuxer,
    &ff_mp3_demuxer,
#if CONFIG_WAV_DEMUXER
    &ff_wav_demuxer,
#endif
    NULL };
