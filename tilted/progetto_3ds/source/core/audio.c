#include "audio.h"

#define AUDIO_BGM_CHANNEL   0
#define NDSP_STREAM_FRAMES  4096

typedef enum {
    AUDIO_BACKEND_NONE = 0,
    AUDIO_BACKEND_NDSP,
    AUDIO_BACKEND_CSND
} AudioBackend;

static AudioBackend s_backend = AUDIO_BACKEND_NONE;
static float        s_bgmVolume = 1.0f;

// Stato NDSP (streaming double-buffer per compatibilita' su hardware ed emulatori)
static ndspWaveBuf  s_waveBufs[2];
static u8*          s_ndspStreamBuf = NULL;
static u32          s_ndspBufBytes = 0;
static volatile bool s_ndspPlaying = false;

static const u8*    s_pcmSrc = NULL;
static u32          s_totalFrames = 0;
static u32          s_cursorFrame = 0;
static u32          s_blockAlign = 0;
static u16          s_bitsPerSample = 0;

// Stato CSND (fallback automatico se manca dspfirm.cdc)
static int          s_csndChL = -1;
static int          s_csndChR = -1;
static u8*          s_csndLeft = NULL;
static u8*          s_csndRight = NULL;
static bool         s_csndStereo = false;

static u16 readU16LE(const u8* p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 readU32LE(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void applyNdspMix(float volume) {
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = volume;
    mix[1] = volume;
    ndspChnSetMix(AUDIO_BGM_CHANNEL, mix);
}

static void applyCsndVolume(float volume) {
    if (s_csndChL >= 0 && s_csndLeft) {
        float panL = s_csndStereo ? -1.0f : 0.0f;
        CSND_SetVol((u32)s_csndChL, CSND_VOL(volume, panL), 0);
    }
    if (s_csndStereo && s_csndChR >= 0 && s_csndRight) {
        CSND_SetVol((u32)s_csndChR, CSND_VOL(volume, 1.0f), 0);
    }
    csndExecCmds(true);
}

static bool parseWavPcm(const u8* data, u32 size, u16* outChannels, u32* outRate, u16* outBits, const u8** outPcm, u32* outPcmSize) {
    if (!data || size < 12) return false;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) return false;

    bool hasFmt = false;
    u16 audioFormat = 0;
    u16 channels = 0;
    u32 sampleRate = 0;
    u16 bitsPerSample = 0;
    const u8* pcmPtr = NULL;
    u32 pcmSize = 0;

    u32 pos = 12;
    while (pos + 8 <= size) {
        const u8* hdr = data + pos;
        u32 chunkSize = readU32LE(hdr + 4);
        u32 dataOffset = pos + 8;
        if (chunkSize > size - dataOffset) break;

        if (memcmp(hdr, "fmt ", 4) == 0 && chunkSize >= 16) {
            audioFormat   = readU16LE(data + dataOffset + 0);
            channels      = readU16LE(data + dataOffset + 2);
            sampleRate    = readU32LE(data + dataOffset + 4);
            bitsPerSample = readU16LE(data + dataOffset + 14);
            hasFmt = true;
        } else if (memcmp(hdr, "data", 4) == 0) {
            pcmPtr = data + dataOffset;
            pcmSize = chunkSize;
        }

        u32 step = chunkSize + (chunkSize & 1U);
        if (step > size - dataOffset) break;
        pos = dataOffset + step;
    }

    if (!hasFmt || audioFormat != 1 || !pcmPtr || pcmSize == 0) return false;
    if (channels != 1 && channels != 2) return false;
    if (bitsPerSample != 8 && bitsPerSample != 16) return false;

    *outChannels = channels;
    *outRate     = sampleRate;
    *outBits     = bitsPerSample;
    *outPcm      = pcmPtr;
    *outPcmSize  = pcmSize;
    return true;
}

static void fillAndQueueNdspBuf(ndspWaveBuf* buf) {
    if (!s_ndspPlaying || !s_pcmSrc || s_totalFrames == 0 || !buf->data_vaddr) return;

    u32 framesLeft = NDSP_STREAM_FRAMES;
    u8* dst = (u8*)buf->data_vaddr;

    while (framesLeft > 0) {
        u32 avail = s_totalFrames - s_cursorFrame;
        u32 take = (framesLeft < avail) ? framesLeft : avail;
        u32 byteOffset = s_cursorFrame * s_blockAlign;
        u32 byteCount = take * s_blockAlign;

        if (s_bitsPerSample == 8) {
            // WAV PCM8 è unsigned [0..255], mentre NDSP PCM8 richiede signed [-128..127]
            for (u32 i = 0; i < byteCount; i++) {
                dst[i] = (u8)((s8)((int)s_pcmSrc[byteOffset + i] - 128));
            }
        } else {
            memcpy(dst, s_pcmSrc + byteOffset, byteCount);
        }

        dst += byteCount;
        framesLeft -= take;
        s_cursorFrame += take;
        if (s_cursorFrame >= s_totalFrames) {
            s_cursorFrame = 0;
        }
    }

    buf->nsamples = NDSP_STREAM_FRAMES;
    DSP_FlushDataCache(buf->data_vaddr, s_ndspBufBytes);
    ndspChnWaveBufAdd(AUDIO_BGM_CHANNEL, buf);
}

static void ndspAudioCallback(void* data) {
    (void)data;
    if (!s_ndspPlaying) return;

    for (int i = 0; i < 2; i++) {
        if (s_waveBufs[i].status == NDSP_WBUF_DONE) {
            fillAndQueueNdspBuf(&s_waveBufs[i]);
        }
    }
}

static bool playNdspStream(u16 channels, u32 sampleRate, u16 bitsPerSample, const u8* pcmSrc, u32 nsamples) {
    u32 blockAlign = (u32)channels * ((u32)bitsPerSample / 8U);
    u32 bufBytes = NDSP_STREAM_FRAMES * blockAlign;

    u8* streamMem = (u8*)linearAlloc(bufBytes * 2U);
    if (!streamMem) return false;

    s_ndspStreamBuf = streamMem;
    s_ndspBufBytes  = bufBytes;
    s_pcmSrc        = pcmSrc;
    s_totalFrames   = nsamples;
    s_cursorFrame   = 0;
    s_blockAlign    = blockAlign;
    s_bitsPerSample = bitsPerSample;

    u16 ndspFmt = (channels == 1)
        ? ((bitsPerSample == 8) ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_MONO_PCM16)
        : ((bitsPerSample == 8) ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_STEREO_PCM16);

    ndspChnReset(AUDIO_BGM_CHANNEL);
    ndspChnSetInterp(AUDIO_BGM_CHANNEL, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(AUDIO_BGM_CHANNEL, (float)sampleRate);
    ndspChnSetFormat(AUDIO_BGM_CHANNEL, ndspFmt);
    applyNdspMix(s_bgmVolume);

    memset(s_waveBufs, 0, sizeof(s_waveBufs));
    s_waveBufs[0].data_vaddr = s_ndspStreamBuf;
    s_waveBufs[1].data_vaddr = s_ndspStreamBuf + bufBytes;

    s_ndspPlaying = true;
    fillAndQueueNdspBuf(&s_waveBufs[0]);
    fillAndQueueNdspBuf(&s_waveBufs[1]);
    ndspSetCallback(ndspAudioCallback, NULL);
    return true;
}

static bool playCsndLoop(u16 channels, u32 sampleRate, u16 bitsPerSample, const u8* pcmSrc, u32 nsamples) {
    if (s_csndChL < 0 || (channels == 2 && s_csndChR < 0)) return false;

    u32 sampleBytes = (u32)bitsPerSample / 8U;
    u32 monoBytes = nsamples * sampleBytes;

    s_csndLeft = (u8*)linearAlloc(monoBytes);
    if (!s_csndLeft) return false;

    if (channels == 2) {
        s_csndRight = (u8*)linearAlloc(monoBytes);
        if (!s_csndRight) {
            linearFree(s_csndLeft);
            s_csndLeft = NULL;
            return false;
        }
    }

    if (channels == 1) {
        if (bitsPerSample == 8) {
            for (u32 i = 0; i < nsamples; i++) {
                s_csndLeft[i] = (u8)((s8)((int)pcmSrc[i] - 128));
            }
        } else {
            memcpy(s_csndLeft, pcmSrc, monoBytes);
        }
    } else if (bitsPerSample == 8) {
        for (u32 i = 0; i < nsamples; i++) {
            s_csndLeft[i]  = (u8)((s8)((int)pcmSrc[i * 2 + 0] - 128));
            s_csndRight[i] = (u8)((s8)((int)pcmSrc[i * 2 + 1] - 128));
        }
    } else {
        const s16* src16 = (const s16*)pcmSrc;
        s16* dstL = (s16*)s_csndLeft;
        s16* dstR = (s16*)s_csndRight;
        for (u32 i = 0; i < nsamples; i++) {
            dstL[i] = src16[i * 2 + 0];
            dstR[i] = src16[i * 2 + 1];
        }
    }

    CSND_FlushDataCache(s_csndLeft, monoBytes);
    if (s_csndRight) {
        CSND_FlushDataCache(s_csndRight, monoBytes);
    }

    s_csndStereo = (channels == 2);
    u32 flags = ((bitsPerSample == 8) ? SOUND_FORMAT_8BIT : SOUND_FORMAT_16BIT) |
                SOUND_REPEAT | SOUND_LINEAR_INTERP;

    if (s_csndStereo) {
        csndPlaySound(s_csndChL, flags, sampleRate, s_bgmVolume, -1.0f, s_csndLeft, s_csndLeft, monoBytes);
        csndPlaySound(s_csndChR, flags, sampleRate, s_bgmVolume,  1.0f, s_csndRight, s_csndRight, monoBytes);
    } else {
        csndPlaySound(s_csndChL, flags, sampleRate, s_bgmVolume,  0.0f, s_csndLeft, s_csndLeft, monoBytes);
    }
    return true;
}

bool audioInit(void) {
    if (s_backend != AUDIO_BACKEND_NONE) return true;

    if (R_SUCCEEDED(ndspInit())) {
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnReset(AUDIO_BGM_CHANNEL);
        s_backend = AUDIO_BACKEND_NDSP;
        return true;
    }

    // Se ndspInit() fallisce (es. dspfirm.cdc assente su SD virtuale o reale), proviamo CSND
    if (R_SUCCEEDED(csndInit())) {
        s_csndChL = -1;
        s_csndChR = -1;
        for (int ch = 0; ch < CSND_NUM_CHANNELS; ch++) {
            if (csndChannels & BIT(ch)) {
                if (s_csndChL < 0) s_csndChL = ch;
                else if (s_csndChR < 0) {
                    s_csndChR = ch;
                    break;
                }
            }
        }
        if (s_csndChL >= 0) {
            s_backend = AUDIO_BACKEND_CSND;
            return true;
        }
        csndExit();
    }

    s_backend = AUDIO_BACKEND_NONE;
    return false;
}

bool audioPlayLoopingWav(const void* wavData, u32 wavSize) {
    if (s_backend == AUDIO_BACKEND_NONE) return false;

    u16 channels = 0;
    u32 sampleRate = 0;
    u16 bitsPerSample = 0;
    const u8* pcmSrc = NULL;
    u32 pcmSize = 0;

    if (!parseWavPcm((const u8*)wavData, wavSize, &channels, &sampleRate, &bitsPerSample, &pcmSrc, &pcmSize)) {
        return false;
    }

    u32 blockAlign = (u32)channels * ((u32)bitsPerSample / 8U);
    u32 nsamples = pcmSize / blockAlign;
    if (nsamples == 0) return false;

    audioStopBgm();

    if (s_backend == AUDIO_BACKEND_NDSP) {
        return playNdspStream(channels, sampleRate, bitsPerSample, pcmSrc, nsamples);
    }
    return playCsndLoop(channels, sampleRate, bitsPerSample, pcmSrc, nsamples);
}

void audioSetBgmVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    s_bgmVolume = volume;

    if (s_backend == AUDIO_BACKEND_NDSP) {
        applyNdspMix(s_bgmVolume);
    } else if (s_backend == AUDIO_BACKEND_CSND) {
        applyCsndVolume(s_bgmVolume);
    }
}

void audioStopBgm(void) {
    if (s_backend == AUDIO_BACKEND_NDSP) {
        s_ndspPlaying = false;
        ndspSetCallback(NULL, NULL);
        ndspChnWaveBufClear(AUDIO_BGM_CHANNEL);
        if (s_ndspStreamBuf) {
            linearFree(s_ndspStreamBuf);
            s_ndspStreamBuf = NULL;
        }
        s_ndspBufBytes = 0;
        s_pcmSrc = NULL;
        s_totalFrames = 0;
        s_cursorFrame = 0;
        memset(s_waveBufs, 0, sizeof(s_waveBufs));
    } else if (s_backend == AUDIO_BACKEND_CSND) {
        if (s_csndChL >= 0) CSND_SetPlayStateR((u32)s_csndChL, 0);
        if (s_csndChR >= 0) CSND_SetPlayStateR((u32)s_csndChR, 0);
        csndExecCmds(true);
        if (s_csndLeft) {
            linearFree(s_csndLeft);
            s_csndLeft = NULL;
        }
        if (s_csndRight) {
            linearFree(s_csndRight);
            s_csndRight = NULL;
        }
        s_csndStereo = false;
    }
}

void audioExit(void) {
    audioStopBgm();
    if (s_backend == AUDIO_BACKEND_NDSP) {
        ndspExit();
    } else if (s_backend == AUDIO_BACKEND_CSND) {
        csndExit();
    }
    s_backend = AUDIO_BACKEND_NONE;
    s_csndChL = -1;
    s_csndChR = -1;
}
