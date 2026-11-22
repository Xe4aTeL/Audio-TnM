#ifndef TRANSCRIPTION_HPP
#define TRANSCRIPTION_HPP

#include <string>
#include <vector>
#include <cassert>
#include <whisper.h>

#include "../../utils/audio_utils.hpp"
#include "../../utils/text_utils.hpp"
namespace TranscribeMethod
{
    struct Model
    {
        enum Models name;
        std::string pathToModel;
    };

    enum Models
    {
        GGML_WHISPER_TINY,
        GGML_WHISPER_BASE,
        GGML_WHISPER_SMALL,
        GGML_WHISPER_MEDIUM,
        GGML_WHISPER_LARGEv2,
        GGML_WHISPER_LARGEv3,
        GGML_WHISPER_LARGEv3_TURBO
    };

    class Transcriber
    {
    public:
        Transcriber(Model &, bool);
        void Transcribe(const std::string &, TextUtils::TextBank &);
        void ChangeModel(Model &);
        void DisableGPU();
        void EnableGPU();

    private:
        Model selected_model;
        bool use_gpu;
    };
}

#endif // TRANSCRIPTION_HPP
