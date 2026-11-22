#include "audio_player.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cstring>

struct PlayerContext
{
    ma_device device;
    ma_device_config deviceConfig;
};

AudioPlayer::AudioPlayer()
    : m_deviceContext(nullptr), m_isInitialized(false), m_isPlaying(false),
      m_volume(0.8f), m_readCursor(0), m_totalSamples(0)
{
    m_deviceContext = new PlayerContext();
}

AudioPlayer::~AudioPlayer()
{
    Stop();
    PlayerContext *ctx = static_cast<PlayerContext *>(m_deviceContext);
    if (m_isInitialized)
        ma_device_uninit(&ctx->device);
    delete ctx;
}

bool AudioPlayer::Initialize(int sampleRate, int channels)
{
    PlayerContext *ctx = static_cast<PlayerContext *>(m_deviceContext);

    ctx->deviceConfig = ma_device_config_init(ma_device_type_playback);
    ctx->deviceConfig.playback.format = ma_format_f32;
    ctx->deviceConfig.playback.channels = channels;
    ctx->deviceConfig.sampleRate = sampleRate;
    ctx->deviceConfig.dataCallback = AudioPlayer::DataCallback;
    ctx->deviceConfig.pUserData = this;

    if (ma_device_init(nullptr, &ctx->deviceConfig, &ctx->device) != MA_SUCCESS)
        return false;

    m_isInitialized = true;
    return true;
}

void AudioPlayer::LoadAudio(const std::vector<std::vector<float>> &channelsData)
{
    Pause();
    m_audioData = channelsData;
    m_readCursor = 0;
    m_totalSamples = 0;

    if (!m_audioData.empty() && !m_audioData[0].empty())
        m_totalSamples = m_audioData[0].size();
    m_trackMutes.assign(m_audioData.size(), false);
}

void AudioPlayer::Play()
{
    if (!m_isInitialized || m_audioData.empty() || m_totalSamples == 0)
        return;
    PlayerContext *ctx = static_cast<PlayerContext *>(m_deviceContext);
    ma_device_start(&ctx->device);
    m_isPlaying = true;
}

void AudioPlayer::Pause()
{
    if (!m_isInitialized)
        return;
    PlayerContext *ctx = static_cast<PlayerContext *>(m_deviceContext);
    ma_device_stop(&ctx->device);
    m_isPlaying = false;
}

void AudioPlayer::Stop()
{
    Pause();
    m_readCursor = 0;
}

void AudioPlayer::SeekToRatio(float ratio)
{
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    m_readCursor = static_cast<size_t>(ratio * m_totalSamples);
}

void AudioPlayer::SeekToFrame(size_t frameIndex)
{
    m_readCursor = (std::min)(frameIndex, m_totalSamples);
}

void AudioPlayer::SetVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioPlayer::SetTrackMute(int trackIdx, bool muted)
{
    if (trackIdx >= 0 && trackIdx < (int)m_trackMutes.size())
        m_trackMutes[trackIdx] = muted;
}

bool AudioPlayer::IsPlaying() const { return m_isPlaying; }
size_t AudioPlayer::GetTotalFrames() const { return m_totalSamples; }
size_t AudioPlayer::GetCurrentFrame() const { return m_readCursor; }

void AudioPlayer::DataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    AudioPlayer *player = static_cast<AudioPlayer *>(pDevice->pUserData);
    if (player)
        player->ReadFrames(static_cast<float *>(pOutput), frameCount);
}

void AudioPlayer::ReadFrames(float *pOutput, ma_uint32 frameCount)
{
    size_t numChannels = m_audioData.size();
    if (numChannels == 0)
    {
        std::memset(pOutput, 0, frameCount * sizeof(float));
        return;
    }

    for (ma_uint32 f = 0; f < frameCount; ++f)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            float sample = 0.0f;
            if (m_readCursor < m_totalSamples && !m_trackMutes[c])
                sample = m_audioData[c][m_readCursor];
            pOutput[f * numChannels + c] = sample * m_volume;
        }

        if (m_readCursor < m_totalSamples)
            m_readCursor++;
    }
}

// This function was heavily inspired by whisper.cpp-cli
void fromFFtoMA(std::vector<uint8_t> &in, std::vector<float> &out)
{
    ma_result result;
    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 1, 16000);
    struct decoder_guard
    {
        ma_decoder decoder;
        bool initialized = false;
        ma_decoder *operator&() { return &decoder; }
        ~decoder_guard()
        {
            if (initialized)
                ma_decoder_uninit(&decoder);
        }
    };
    decoder_guard decoder{};
    result = ma_decoder_init_memory(in.data(), in.size(), &decoder_config, &decoder);
    if (result != MA_SUCCESS)
        return;

    ma_uint64 frame_count;
    ma_uint64 frames_read;

    if (result != MA_SUCCESS)
        return;
    if ((result = ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count)) != MA_SUCCESS)
        return;

    out.resize(frame_count);

    if ((result = ma_decoder_read_pcm_frames(&decoder, out.data(), frame_count, &frames_read)) != MA_SUCCESS)
        return;
}
