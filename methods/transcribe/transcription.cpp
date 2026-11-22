#include "transcription.hpp"

namespace TranscribeMethod
{
    Transcriber::Transcriber(Model &selected_model, bool use_gpu = 0)
    {
        this->selected_model = selected_model;
        this->use_gpu = use_gpu;
    }

    void Transcriber::Transcribe(const std::string &filename, TextUtils::TextBank &out)
    {
        std::vector<float> pcmf32; // mono-channel F32 PCM
        int n_segments;
        struct whisper_context_params cparams;
        struct whisper_context *ctx;
        whisper_full_params wparams;

        if ((selected_model.name == GGML_WHISPER_TINY) |
            (selected_model.name == GGML_WHISPER_BASE) |
            (selected_model.name == GGML_WHISPER_SMALL) |
            (selected_model.name == GGML_WHISPER_MEDIUM) |
            (selected_model.name == GGML_WHISPER_LARGEv2) |
            (selected_model.name == GGML_WHISPER_LARGEv3) |
            (selected_model.name == GGML_WHISPER_LARGEv3_TURBO))
        {
            pcmf32 = AudioUtils::GetPCMData(filename);

            // Load whisper.cpp
            ggml_backend_load_all();
            cparams = whisper_context_default_params();

            // TODO: select the appropirate params including model
            cparams.use_gpu = use_gpu;
            cparams.gpu_device = 0;
            cparams.flash_attn = true;
            // cparams.dtw_aheads_preset = WHISPER_AHEADS_LARGE_V3_TURBO;

            ctx = whisper_init_from_file_with_params(selected_model.pathToModel.c_str(), cparams);
            if (!ctx)
                return;

            wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            // wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
            wparams.print_realtime = false;
            wparams.print_progress = false;
            wparams.print_timestamps = false;
            wparams.print_special = false;
            wparams.translate = false;
            wparams.language = "auto"; // for auto-detection, set to nullptr, "" or "auto"
            wparams.detect_language = false;
            wparams.n_threads = 1;
            wparams.greedy.best_of = 5;
            wparams.thold_pt = 0.01f;
            // wparams.initial_prompt = std::string.c_str();

            // Transcribe
            if (whisper_full_parallel(ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size()), 1) != 0)
            {
                whisper_free(ctx);
                return;
            }

            n_segments = whisper_full_n_segments(ctx);
            for (int i = 0; i < n_segments; ++i)
            {
                int64_t t0 = whisper_full_get_segment_t0(ctx, i);
                int64_t t1 = whisper_full_get_segment_t1(ctx, i);
                for (int j = 0; j < whisper_full_n_tokens(ctx, i); ++j)
                {
                    const char *text = whisper_full_get_token_text(ctx, i, j);
                    const float p = whisper_full_get_token_p(ctx, i, j);
                    out.LoadWordFromTranscription(text, p, t0, t1);
                }
            }

            // TODO: make callbacks to check on progress and not lock the main thread?

            // Free whisper context
            whisper_free(ctx);
            return;
        }

        // Unknown model
        out.LoadWordFromTranscription("", 0.0f, -1, -1);
    }

    void Transcriber::ChangeModel(Model &model_name)
    {
        this->selected_model = model_name;
    }

    void Transcriber::DisableGPU()
    {
        this->use_gpu = 0;
    }

    void Transcriber::EnableGPU()
    {
        this->use_gpu = 1;
    }
}
