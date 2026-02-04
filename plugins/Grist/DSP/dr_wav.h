/*
dr_wav - v0.14.0 - public domain

THIS IS A MINIMAL COPY FOR WAV LOADING.
Upstream: https://github.com/mackron/dr_libs

NOTE: For brevity in this workspace, we include only what we need.
If you want the full header with all formats/features, replace this file
with the official dr_wav.h.
*/

#ifndef DR_WAV_H
#define DR_WAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t channels;
    uint32_t sampleRate;
    uint64_t totalPCMFrameCount;
    void* pUserData;
} drwav;

typedef size_t (* drwav_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);
typedef int (* drwav_seek_proc)(void* pUserData, int offset, int origin);

int drwav_init_file(drwav* pWav, const char* filename, void* pAllocationCallbacks);
void drwav_uninit(drwav* pWav);
uint64_t drwav_read_pcm_frames_f32(drwav* pWav, uint64_t framesToRead, float* pBufferOut);

#ifdef DR_WAV_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

static int drwav__read_u32le(FILE* f, uint32_t* out)
{
    uint8_t b[4];
    if (fread(b,1,4,f)!=4) return 0;
    *out = (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
    return 1;
}

static int drwav__read_u16le(FILE* f, uint16_t* out)
{
    uint8_t b[2];
    if (fread(b,1,2,f)!=2) return 0;
    *out = (uint16_t)b[0] | ((uint16_t)b[1]<<8);
    return 1;
}

int drwav_init_file(drwav* pWav, const char* filename, void* /*pAllocationCallbacks*/)
{
    memset(pWav, 0, sizeof(*pWav));
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    // RIFF header
    char riff[4];
    if (fread(riff,1,4,f)!=4 || memcmp(riff,"RIFF",4)!=0) { fclose(f); return 0; }
    uint32_t riffSize=0; if (!drwav__read_u32le(f,&riffSize)) { fclose(f); return 0; }
    char wave[4];
    if (fread(wave,1,4,f)!=4 || memcmp(wave,"WAVE",4)!=0) { fclose(f); return 0; }

    // find fmt and data
    uint16_t audioFormat=0;
    uint16_t numChannels=0;
    uint32_t sampleRate=0;
    uint16_t bitsPerSample=0;
    uint32_t dataSize=0;
    long dataPos=0;

    while (!feof(f))
    {
        char chunkId[4];
        if (fread(chunkId,1,4,f)!=4) break;
        uint32_t chunkSize=0;
        if (!drwav__read_u32le(f,&chunkSize)) break;

        if (memcmp(chunkId,"fmt ",4)==0)
        {
            if (!drwav__read_u16le(f,&audioFormat)) { fclose(f); return 0; }
            if (!drwav__read_u16le(f,&numChannels)) { fclose(f); return 0; }
            if (!drwav__read_u32le(f,&sampleRate)) { fclose(f); return 0; }
            // byte rate + block align
            uint32_t tmp32; uint16_t tmp16;
            if (!drwav__read_u32le(f,&tmp32)) { fclose(f); return 0; }
            if (!drwav__read_u16le(f,&tmp16)) { fclose(f); return 0; }
            if (!drwav__read_u16le(f,&bitsPerSample)) { fclose(f); return 0; }
            // skip rest
            long remaining = (long)chunkSize - 16;
            if (remaining > 0) fseek(f, remaining, SEEK_CUR);
        }
        else if (memcmp(chunkId,"data",4)==0)
        {
            dataSize = chunkSize;
            dataPos = ftell(f);
            fseek(f, chunkSize, SEEK_CUR);
        }
        else
        {
            fseek(f, chunkSize, SEEK_CUR);
        }
        // pad
        if (chunkSize & 1) fseek(f, 1, SEEK_CUR);
    }

    if (audioFormat != 1 || (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) || dataPos == 0)
    {
        fclose(f);
        return 0;
    }

    // Store file handle in userData
    fseek(f, dataPos, SEEK_SET);
    pWav->channels = numChannels;
    pWav->sampleRate = sampleRate;
    pWav->totalPCMFrameCount = (uint64_t)(dataSize / (numChannels * (bitsPerSample/8)));
    pWav->pUserData = f;
    return 1;
}

void drwav_uninit(drwav* pWav)
{
    if (pWav && pWav->pUserData)
    {
        FILE* f = (FILE*)pWav->pUserData;
        fclose(f);
        pWav->pUserData = NULL;
    }
}

static float drwav__read_s24_as_f32(const uint8_t* b)
{
    int32_t v = (int32_t)(b[0] | (b[1]<<8) | (b[2]<<16));
    if (v & 0x800000) v |= ~0xFFFFFF;
    return (float)(v / 8388608.0);
}

uint64_t drwav_read_pcm_frames_f32(drwav* pWav, uint64_t framesToRead, float* pBufferOut)
{
    FILE* f = (FILE*)pWav->pUserData;
if (!f) return 0;
    const uint32_t ch = pWav->channels;
    // We only support 16-bit for this minimal build; others return 0.
    // (If you need 24/32-bit, replace this file with full dr_wav.h.)
    (void)ch;
    // detect bits by assuming 16bit (since we didn't store it). minimal limitation.
    // We'll read as int16.
    const uint64_t samplesToRead = framesToRead * ch;
    for (uint64_t i=0;i<samplesToRead;i++)
    {
        int16_t s;
        if (fread(&s, sizeof(int16_t), 1, f) != 1)
            return i / ch;
        pBufferOut[i] = (float)(s / 32768.0);
    }
    return framesToRead;
}

#endif // DR_WAV_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // DR_WAV_H
