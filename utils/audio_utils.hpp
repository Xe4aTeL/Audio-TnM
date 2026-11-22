#ifndef AUDIO_UTILS_HPP
#define AUDIO_UTILS_HPP

#include <string>
#include <vector>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
}

#include "audio_player.hpp"

namespace AudioUtils
{
    struct AudioInfo
    {
        std::string filename;
        std::string filepath;
        int channels;
        int sampleRate;
        int bitDepth;
    };

    AudioInfo GetAudioInfo(const std::string &filepath);
    std::vector<float> GetWaveformData(const std::string &filepath, int channel);
    std::vector<std::vector<float>> GetAllChannelSamples(const std::string &filepath);
    std::vector<float> GetPCMData(const std::string &filepath);

    bool ResampleAudio(const std::string &filepath, const uint8_t outFormat, std::vector<uint8_t> &out);
}

#endif // AUDIO_UTILS_HPP
