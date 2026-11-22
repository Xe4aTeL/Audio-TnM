#ifndef AUDIO_PLAYER_HPP
#define AUDIO_PLAYER_HPP

#include <vector>
#include <string>

#include "miniaudio.h"

class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();

    bool Initialize(int sampleRate, int channels);
    void LoadAudio(const std::vector<std::vector<float>> &channelsData);

    void Play();
    void Pause();
    void Stop();
    void SeekToRatio(float ratio); // 0.0 to 1.0
    void SeekToFrame(size_t frameIndex);
    void SetVolume(float volume); // 0.0 to 1.0
    void SetTrackMute(int trackIdx, bool muted);

    bool IsPlaying() const;
    size_t GetTotalFrames() const;
    size_t GetCurrentFrame() const;

private:
    static void DataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);
    void ReadFrames(float *pOutput, ma_uint32 frameCount);

    void *m_deviceContext; // Opaque pointer hiding miniaudio struct definitions
    bool m_isInitialized;
    bool m_isPlaying;
    float m_volume;

    std::vector<std::vector<float>> m_audioData; // [channel][samples]
    std::vector<bool> m_trackMutes;
    size_t m_readCursor;
    size_t m_totalSamples;
};

void fromFFtoMA(std::vector<uint8_t> &in, std::vector<float> &out);

#endif // AUDIO_PLAYER_HPP
