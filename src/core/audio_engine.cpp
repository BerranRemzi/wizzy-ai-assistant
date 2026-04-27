#include "audio_engine.h"
#include "config.h"

Audio audio(true, I2S_DAC_CHANNEL_LEFT_EN);

void audio_engine_stop_soft()
{
    audio.stopSong();
    audio.freeDecoderMemory();
    audio.clearDmaBuffer();
}

void audio_engine_prepare_start()
{
    audio_engine_stop_soft_if_active();
    audio.setVolume(0);
    audio.clearDmaBuffer();
    delay(AUDIO_DMA_SETTLE_MS);
}

void audio_engine_stop_soft_if_active()
{
    if (!audio.isRunning() && audio.inBufferFilled() == 0) return;
    audio_engine_stop_soft();
}

uint8_t audio_engine_get_volume() { return audio.getVolume(); }
