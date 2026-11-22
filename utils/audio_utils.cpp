#include "audio_utils.hpp"
#include <cmath>
#include <random>
#include <fstream>

namespace AudioUtils
{
    AudioInfo GetAudioInfo(const std::string &filepath)
    {
        AudioInfo info = {"", "", 0, 0, 0};

        // Extract filename
        size_t slashPos = filepath.find_last_of("/\\");
        info.filename = (slashPos == std::string::npos) ? filepath : filepath.substr(slashPos + 1);
        info.filepath = filepath;

        AVFormatContext *formatContext = nullptr;
        if (avformat_open_input(&formatContext, filepath.c_str(), nullptr, nullptr) != 0)
        {
            return info;
        }

        if (avformat_find_stream_info(formatContext, nullptr) >= 0)
        {
            int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (streamIndex >= 0)
            {
                AVCodecParameters *codecPar = formatContext->streams[streamIndex]->codecpar;

                info.channels = codecPar->ch_layout.nb_channels;
                info.sampleRate = codecPar->sample_rate;

                // Try to get exact bit depth, fallback to bytes_per_sample * 8
                info.bitDepth = codecPar->bits_per_raw_sample;
                if (info.bitDepth == 0)
                    info.bitDepth = av_get_bytes_per_sample(static_cast<AVSampleFormat>(codecPar->format)) * 8;
            }
        }

        avformat_close_input(&formatContext);
        return info;
    }

    std::vector<float> GetWaveformData(const std::string &filepath, int channel)
    {
        std::vector<float> waveformPeaks;
        int target_channel = channel - 1;
        if (target_channel < 0)
            target_channel = 0;

        AVFormatContext *formatContext = nullptr;
        if (avformat_open_input(&formatContext, filepath.c_str(), nullptr, nullptr) != 0)
            return waveformPeaks;
        if (avformat_find_stream_info(formatContext, nullptr) < 0)
        {
            avformat_close_input(&formatContext);
            return waveformPeaks;
        }

        const AVCodec *codec = nullptr;
        int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (streamIndex < 0)
        {
            avformat_close_input(&formatContext);
            return waveformPeaks;
        }

        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecContext, formatContext->streams[streamIndex]->codecpar);
        if (avcodec_open2(codecContext, codec, nullptr) < 0)
        {
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return waveformPeaks;
        }

        SwrContext *swrContext = nullptr;
        swr_alloc_set_opts2(&swrContext,
                            &codecContext->ch_layout, AV_SAMPLE_FMT_FLTP, codecContext->sample_rate,
                            &codecContext->ch_layout, codecContext->sample_fmt, codecContext->sample_rate,
                            0, nullptr);
        swr_init(swrContext);

        // Collect all float samples for the target channel
        std::vector<float> rawSamples;
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();

        while (av_read_frame(formatContext, packet) >= 0)
        {
            if (packet->stream_index == streamIndex && avcodec_send_packet(codecContext, packet) == 0)
            {
                while (avcodec_receive_frame(codecContext, frame) == 0)
                {
                    uint8_t **outData = nullptr;
                    int outLinesize;
                    av_samples_alloc_array_and_samples(&outData, &outLinesize, codecContext->ch_layout.nb_channels, frame->nb_samples, AV_SAMPLE_FMT_FLTP, 0);
                    swr_convert(swrContext, outData, frame->nb_samples, const_cast<const uint8_t **>(frame->data), frame->nb_samples);

                    if (target_channel < codecContext->ch_layout.nb_channels)
                    {
                        float *channelData = reinterpret_cast<float *>(outData[target_channel]);
                        rawSamples.insert(rawSamples.end(), channelData, channelData + frame->nb_samples);
                    }

                    av_freep(&outData[0]);
                    av_freep(&outData);
                }
            }
            av_packet_unref(packet);
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swrContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);

        if (rawSamples.empty())
            return waveformPeaks;

        // Downsample raw samples into a fixed number of display points (2000 points)
        const int TARGET_POINTS = 2000;
        waveformPeaks.reserve(TARGET_POINTS);

        size_t samplesPerPoint = (std::max)(size_t(1), rawSamples.size() / TARGET_POINTS);

        for (size_t i = 0; i < TARGET_POINTS; ++i)
        {
            size_t start = i * samplesPerPoint;
            if (start >= rawSamples.size())
                break;
            size_t end = (std::min)(rawSamples.size(), start + samplesPerPoint);

            float maxPeak = 0.0f;
            for (size_t j = start; j < end; ++j)
                maxPeak = (std::max)(maxPeak, std::abs(rawSamples[j]));
            waveformPeaks.push_back(maxPeak);
        }

        return waveformPeaks;
    }

    std::vector<std::vector<float>> GetAllChannelSamples(const std::string &filepath)
    {
        std::vector<std::vector<float>> allChannelsData;

        AVFormatContext *formatContext = nullptr;
        if (avformat_open_input(&formatContext, filepath.c_str(), nullptr, nullptr) != 0)
            return allChannelsData;
        if (avformat_find_stream_info(formatContext, nullptr) < 0)
        {
            avformat_close_input(&formatContext);
            return allChannelsData;
        }

        const AVCodec *codec = nullptr;
        int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (streamIndex < 0)
        {
            avformat_close_input(&formatContext);
            return allChannelsData;
        }

        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecContext, formatContext->streams[streamIndex]->codecpar);
        if (avcodec_open2(codecContext, codec, nullptr) < 0)
        {
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return allChannelsData;
        }

        SwrContext *swrContext = nullptr;
        swr_alloc_set_opts2(&swrContext,
                            &codecContext->ch_layout, AV_SAMPLE_FMT_FLTP, codecContext->sample_rate,
                            &codecContext->ch_layout, codecContext->sample_fmt, codecContext->sample_rate,
                            0, nullptr);
        swr_init(swrContext);

        int numChannels = codecContext->ch_layout.nb_channels;
        allChannelsData.resize(numChannels);

        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();

        while (av_read_frame(formatContext, packet) >= 0)
        {
            if (packet->stream_index == streamIndex && avcodec_send_packet(codecContext, packet) == 0)
            {
                while (avcodec_receive_frame(codecContext, frame) == 0)
                {
                    uint8_t **outData = nullptr;
                    int outLinesize;
                    av_samples_alloc_array_and_samples(&outData, &outLinesize, numChannels, frame->nb_samples, AV_SAMPLE_FMT_FLTP, 0);
                    swr_convert(swrContext, outData, frame->nb_samples, const_cast<const uint8_t **>(frame->data), frame->nb_samples);

                    for (int c = 0; c < numChannels; ++c)
                    {
                        float *channelData = reinterpret_cast<float *>(outData[c]);
                        allChannelsData[c].insert(allChannelsData[c].end(), channelData, channelData + frame->nb_samples);
                    }

                    av_freep(&outData[0]);
                    av_freep(&outData);
                }
            }
            av_packet_unref(packet);
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swrContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);

        return allChannelsData;
    }

    std::vector<float> GetPCMData(const std::string &filepath)
    {
        std::vector<float> result;

        // The decoding and transcoding is heavily inspired by whisper.cpp-cli
        AVFormatContext *fmt_ctx = nullptr;
        const AVCodec *decoder = nullptr;
        if (avformat_open_input(&fmt_ctx, filepath.c_str(), nullptr, nullptr) < 0)
            return result;
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
        {
            avformat_close_input(&fmt_ctx);
            return result;
        }
        int audioStreamIdx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
        if (audioStreamIdx < 0)
        {
            avformat_close_input(&fmt_ctx);
            return result;
        }
        AVCodecContext *dec_ctx = avcodec_alloc_context3(decoder);
        if (!dec_ctx)
        {
            avformat_close_input(&fmt_ctx);
            return result;
        }
        avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[audioStreamIdx]->codecpar);
        if (avcodec_open2(dec_ctx, decoder, nullptr) < 0)
        {
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&fmt_ctx);
            return result;
        }
        AVChannelLayout targetChLayout = AV_CHANNEL_LAYOUT_MONO;
        AVSampleFormat targetSampleFmt = AV_SAMPLE_FMT_S16;
        uint32_t targetSampleRate = 16000;
        SwrContext *swr_ctx = nullptr;
        swr_alloc_set_opts2(&swr_ctx,
                            &targetChLayout, targetSampleFmt, targetSampleRate,
                            &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate,
                            0, nullptr);
        if (!swr_ctx || swr_init(swr_ctx) < 0)
        {
            av_channel_layout_uninit(&targetChLayout);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&fmt_ctx);
            return result;
        }

        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        std::vector<int16_t> pcm_data;
        const int max_out_samples = 16 * 1024;
        std::vector<int16_t> out_buffer(max_out_samples);

        while (av_read_frame(fmt_ctx, packet) >= 0)
        {
            if (packet->stream_index != audioStreamIdx)
            {
                av_packet_unref(packet);
                continue;
            }

            int ret = avcodec_send_packet(dec_ctx, packet);
            av_packet_unref(packet);

            if (ret < 0)
                continue;

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    break;

                // Resample
                int64_t out_samples = av_rescale_rnd(swr_get_delay(swr_ctx, targetSampleRate) + frame->nb_samples,
                                                     targetSampleRate, targetSampleRate, AV_ROUND_UP);
                if (out_samples > (int)out_buffer.size())
                    out_buffer.resize(out_samples);

                const uint8_t *in_data[16] = {0};
                for (int p = 0; p < (int)dec_ctx->ch_layout.nb_channels && p < 16; p++)
                    in_data[p] = frame->data[p];
                uint8_t *out_data[16] = {0};
                out_data[0] = (uint8_t *)out_buffer.data();

                int got_samples = swr_convert(swr_ctx, out_data, static_cast<int>(out_samples), in_data, frame->nb_samples);
                if (got_samples > 0)
                    pcm_data.insert(pcm_data.end(), out_buffer.begin(), out_buffer.begin() + got_samples);
            }
        }

        // Flush the decoder
        avcodec_send_packet(dec_ctx, nullptr);
        while (avcodec_receive_frame(dec_ctx, frame) >= 0)
        {
            int64_t out_samples = av_rescale_rnd(swr_get_delay(swr_ctx, targetSampleRate) + frame->nb_samples,
                                                 targetSampleRate, targetSampleRate, AV_ROUND_UP);
            if (out_samples > (int)out_buffer.size())
                out_buffer.resize(out_samples);
            const uint8_t *in_data[16] = {0};
            for (int p = 0; p < (int)dec_ctx->ch_layout.nb_channels && p < 16; p++)
                in_data[p] = frame->data[p];
            uint8_t *out_data[16] = {0};
            out_data[0] = (uint8_t *)out_buffer.data();

            int got_samples = swr_convert(swr_ctx, out_data, static_cast<int>(out_samples), in_data, frame->nb_samples);
            if (got_samples > 0)
                pcm_data.insert(pcm_data.end(), out_buffer.begin(), out_buffer.begin() + got_samples);
        }

        // Flush the resampler
        uint8_t *out_data[16] = {0};
        out_data[0] = (uint8_t *)out_buffer.data();
        int flush_samples = swr_convert(swr_ctx, out_data, max_out_samples, nullptr, 0);
        if (flush_samples > 0)
            pcm_data.insert(pcm_data.end(), out_buffer.begin(), out_buffer.begin() + flush_samples);

        // Build WAV output
        uint32_t data_size = static_cast<uint32_t>(pcm_data.size()) * sizeof(int16_t);
        std::vector<uint8_t> wav_data;
        wav_data.resize(44 + data_size);

        // RIFF header
        memcpy(wav_data.data(), "RIFF", 4);
        uint32_t chunk_size = 36 + data_size;
        memcpy(wav_data.data() + 4, &chunk_size, 4);
        memcpy(wav_data.data() + 8, "WAVE", 4);

        // fmt subchunk
        memcpy(wav_data.data() + 12, "fmt ", 4);
        uint32_t subchunk1_size = 16;
        memcpy(wav_data.data() + 16, &subchunk1_size, 4);
        uint16_t audio_format = 1; // PCM
        memcpy(wav_data.data() + 20, &audio_format, 2);
        uint16_t channel_number = 1;
        memcpy(wav_data.data() + 22, &channel_number, 2);
        memcpy(wav_data.data() + 24, &targetSampleRate, 4);

        int bytes_per_sample = (16 / 8) * 1;
        int byte_rate = targetSampleRate * bytes_per_sample;
        memcpy(wav_data.data() + 28, &byte_rate, 4);
        memcpy(wav_data.data() + 32, &bytes_per_sample, 2);
        uint16_t bitPerSample = 16;
        memcpy(wav_data.data() + 34, &bitPerSample, 2);

        // data subchunk
        memcpy(wav_data.data() + 36, "data", 4);
        memcpy(wav_data.data() + 40, &data_size, 4);
        memcpy(wav_data.data() + 44, pcm_data.data(), data_size);

        // Cleanup
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swr_ctx);
        av_channel_layout_uninit(&targetChLayout);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);

        // For debug
        // std::ofstream outFile("output.wav", std::ios::binary);
        // if (!wav_data.empty()) {
        //     outFile.write(reinterpret_cast<const char*>(wav_data.data()), static_cast<std::streamsize>(wav_data.size()));
        // }

        fromFFtoMA(wav_data, result);

        return result;
    }

    struct BufferWriteData
    {
        std::vector<uint8_t> *buffer;
    };

    // Updated signature using const uint8_t* to match modern FFmpeg headers
    static int memory_write_packet(void *opaque, const uint8_t *buf, int buf_size)
    {
        auto *data = reinterpret_cast<BufferWriteData *>(opaque);
        if (data && data->buffer)
        {
            data->buffer->insert(data->buffer->end(), buf, buf + buf_size);
        }
        return buf_size;
    }

    bool ResampleAudio(const std::string &filepath, const uint8_t outFormat, std::vector<uint8_t> &out)
    {
        out.clear();

        AVFormatContext *ifmt_ctx = nullptr;
        if (avformat_open_input(&ifmt_ctx, filepath.c_str(), nullptr, nullptr) < 0)
            return false;
        if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
        {
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        int audio_stream_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_stream_idx < 0)
        {
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        AVCodecParameters *in_codecpar = ifmt_ctx->streams[audio_stream_idx]->codecpar;
        const AVCodec *decoder = avcodec_find_decoder(in_codecpar->codec_id);
        if (!decoder)
        {
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        AVCodecContext *dec_ctx = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(dec_ctx, in_codecpar);
        if (avcodec_open2(dec_ctx, decoder, nullptr) < 0)
        {
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        // Select Encoder & Container Format
        std::string enc_name = outFormat ? "libopus" : "libmp3lame";
        std::string mux_format = outFormat ? "ogg" : "mp3";

        const AVCodec *encoder = avcodec_find_encoder_by_name(enc_name.c_str());
        if (!encoder)
        {
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        AVCodecContext *enc_ctx = avcodec_alloc_context3(encoder);
        enc_ctx->sample_rate = 48000;
        enc_ctx->sample_fmt = outFormat ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_FLTP;
        enc_ctx->bit_rate = 128000;
        enc_ctx->time_base = AVRational{1, enc_ctx->sample_rate};
        int input_channels = dec_ctx->ch_layout.nb_channels > 0 ? dec_ctx->ch_layout.nb_channels : 2;
        if (!outFormat)
        {
            int target_channels = (input_channels > 1) ? 2 : 1;
            av_channel_layout_default(&enc_ctx->ch_layout, target_channels);
        }
        else
        {
            if (dec_ctx->ch_layout.nb_channels > 0)
                av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
            else
                av_channel_layout_default(&enc_ctx->ch_layout, 2);
        }

        AVFormatContext *ofmt_ctx = nullptr;
        if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, mux_format.c_str(), nullptr) < 0)
        {
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!out_stream)
        {
            avformat_free_context(ofmt_ctx);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(enc_ctx, encoder, nullptr) < 0)
        {
            avformat_free_context(ofmt_ctx);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
        out_stream->time_base = enc_ctx->time_base;

        const int io_buffer_size = 32768;
        uint8_t *io_buffer = static_cast<uint8_t *>(av_malloc(io_buffer_size));
        BufferWriteData write_data{&out};

        AVIOContext *avio_ctx = avio_alloc_context(
            io_buffer, io_buffer_size, 1, &write_data,
            nullptr,
            reinterpret_cast<int (*)(void *, const uint8_t *, int)>(memory_write_packet),
            nullptr);
        ofmt_ctx->pb = avio_ctx;

        if (avformat_write_header(ofmt_ctx, nullptr) < 0)
        {
            avio_context_free(&avio_ctx);
            avformat_free_context(ofmt_ctx);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        SwrContext *swr_ctx = nullptr;
        if (swr_alloc_set_opts2(&swr_ctx,
                                &enc_ctx->ch_layout, enc_ctx->sample_fmt, enc_ctx->sample_rate,
                                &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate,
                                0, nullptr) < 0 ||
            swr_init(swr_ctx) < 0)
        {
            avio_context_free(&avio_ctx);
            avformat_free_context(ofmt_ctx);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&ifmt_ctx);
            return false;
        }

        AVAudioFifo *fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->ch_layout.nb_channels, 1);

        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();

        int64_t pts_counter = 0;
        bool success = true;

        auto encode_write = [&](AVFrame *frm)
        {
            if (frm)
            {
                frm->pts = pts_counter;
                pts_counter += frm->nb_samples;
            }
            int ret = avcodec_send_frame(enc_ctx, frm);
            if (ret < 0)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                return false;
            }

            AVPacket *enc_pkt = av_packet_alloc();
            while (avcodec_receive_packet(enc_ctx, enc_pkt) == 0)
            {
                enc_pkt->stream_index = out_stream->index;
                av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, out_stream->time_base);
                av_interleaved_write_frame(ofmt_ctx, enc_pkt);
                av_packet_unref(enc_pkt);
            }
            av_packet_free(&enc_pkt);
            return true;
        };

        while (av_read_frame(ifmt_ctx, packet) >= 0)
        {
            if (packet->stream_index == audio_stream_idx && avcodec_send_packet(dec_ctx, packet) == 0)
            {
                while (avcodec_receive_frame(dec_ctx, frame) == 0)
                {
                    int64_t out_samples = av_rescale_rnd(
                        swr_get_delay(swr_ctx, dec_ctx->sample_rate) + frame->nb_samples,
                        enc_ctx->sample_rate,
                        dec_ctx->sample_rate,
                        AV_ROUND_UP);

                    AVFrame *resampled_frame = av_frame_alloc();
                    resampled_frame->format = enc_ctx->sample_fmt;
                    av_channel_layout_copy(&resampled_frame->ch_layout, &enc_ctx->ch_layout);
                    resampled_frame->sample_rate = enc_ctx->sample_rate;
                    resampled_frame->nb_samples = static_cast<int>(out_samples);

                    int buf_ret = av_frame_get_buffer(resampled_frame, 0);
                    if (buf_ret < 0)
                    {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                        av_strerror(buf_ret, errbuf, sizeof(errbuf));
                        av_frame_free(&resampled_frame);
                        success = false;
                        break;
                    }

                    int converted_count = swr_convert(
                        swr_ctx,
                        resampled_frame->data, resampled_frame->nb_samples,
                        const_cast<const uint8_t **>(frame->data), frame->nb_samples);

                    if (converted_count < 0)
                    {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                        av_strerror(converted_count, errbuf, sizeof(errbuf));
                        av_frame_free(&resampled_frame);
                        success = false;
                        break;
                    }

                    if (av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + converted_count) < 0)
                    {
                        av_frame_free(&resampled_frame);
                        success = false;
                        break;
                    }
                    av_audio_fifo_write(fifo, reinterpret_cast<void **>(resampled_frame->data), converted_count);

                    int frame_size = (enc_ctx->frame_size > 0) ? enc_ctx->frame_size : 1152;
                    while (av_audio_fifo_size(fifo) >= frame_size)
                    {
                        AVFrame *enc_frame = av_frame_alloc();
                        enc_frame->format = enc_ctx->sample_fmt;
                        av_channel_layout_copy(&enc_frame->ch_layout, &enc_ctx->ch_layout);
                        enc_frame->sample_rate = enc_ctx->sample_rate;
                        enc_frame->nb_samples = frame_size;

                        if (av_frame_get_buffer(enc_frame, 0) < 0)
                        {
                            av_frame_free(&enc_frame);
                            success = false;
                            break;
                        }

                        av_audio_fifo_read(fifo, reinterpret_cast<void **>(enc_frame->data), frame_size);

                        if (!encode_write(enc_frame))
                        {
                            av_frame_free(&enc_frame);
                            success = false;
                            break;
                        }
                        av_frame_free(&enc_frame);
                    }

                    av_frame_free(&resampled_frame);
                }
            }
            av_packet_unref(packet);
            if (!success)
                break;
        }

        if (success && av_audio_fifo_size(fifo) > 0)
        {
            int remaining = av_audio_fifo_size(fifo);
            AVFrame *enc_frame = av_frame_alloc();
            enc_frame->format = enc_ctx->sample_fmt;
            av_channel_layout_copy(&enc_frame->ch_layout, &enc_ctx->ch_layout);
            enc_frame->sample_rate = enc_ctx->sample_rate;
            enc_frame->nb_samples = remaining;

            if (av_frame_get_buffer(enc_frame, 0) >= 0)
            {
                av_audio_fifo_read(fifo, reinterpret_cast<void **>(enc_frame->data), remaining);
                encode_write(enc_frame);
            }
            av_frame_free(&enc_frame);
        }

        if (success)
        {
            encode_write(nullptr);
            av_write_trailer(ofmt_ctx);
        }

        av_audio_fifo_free(fifo);

        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swr_ctx);

        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&ifmt_ctx);

        avio_context_free(&avio_ctx);
        avformat_free_context(ofmt_ctx);

        return success;
    }
}
