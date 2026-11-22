#include "modify.hpp"

namespace ModificationMethod
{
    namespace
    {
        constexpr const char *kMetaStartMarker = "__AUDE_META__:";
        constexpr size_t kMaxMetadataBytes = 65536; // 64 KB sanity limit when scanning backwards
    }

    // Static helper callback for AVIO memory writing (avoids lambda reinterpret_cast errors)
    struct EncryptBufferWriteData
    {
        std::vector<uint8_t> *buffer;
    };

    static int encrypt_memory_write_packet(void *opaque, const uint8_t *buf, int buf_size)
    {
        auto *data = reinterpret_cast<EncryptBufferWriteData *>(opaque);
        if (data && data->buffer)
            data->buffer->insert(data->buffer->end(), buf, buf + buf_size);
        return buf_size;
    }

    static size_t GetAudioPacketHeaderSkip(Format fmt, const uint8_t *data, int size)
    {
        if (!data || size <= 0)
            return 0;

        if (fmt == AUDIO_MP3)
            return (size > 4) ? 4 : static_cast<size_t>(size);

        if (fmt == AUDIO_OPUS)
        {
            uint8_t toc = data[0];
            uint8_t code = toc & 0x03;

            if (code == 0 || code == 1)
            {
                return 1;
            }
            if (code == 2)
            {
                if (size < 2)
                    return static_cast<size_t>(size);
                size_t hdr = (data[1] < 252) ? 2 : 3;
                return (std::min)(hdr, static_cast<size_t>(size));
            }

            // code == 3: [TOC][CountByte] + optional padding bytes + optional (M - 1) VBR frame lengths
            if (size < 2)
                return static_cast<size_t>(size);
            uint8_t countByte = data[1];
            bool isVbr = (countByte & 0x80) != 0;
            bool hasPadding = (countByte & 0x40) != 0;
            uint8_t frameCount = countByte & 0x3F;

            int pos = 2;
            if (hasPadding)
            {
                while (pos < size)
                {
                    uint8_t pad = data[pos++];
                    if (pad < 255)
                        break;
                }
            }
            if (isVbr && frameCount > 1)
            {
                for (uint8_t i = 0; i < frameCount - 1 && pos < size; ++i)
                {
                    uint8_t lenByte = data[pos++];
                    if (lenByte >= 252 && pos < size)
                        pos++;
                }
            }
            return static_cast<size_t>((std::min)(pos, size));
        }

        return 0;
    }

    bool Encryptor::DoEncrypt(const std::string &filepath, const std::string &pass, Format outFormat, std::vector<TrackZones> tracks, std::vector<uint8_t> &outBuf)
    {
        if (tracks.empty() || pass.empty() || filepath.empty() || !isInit)
        {
            return false;
        }
        /*
            The general process looks like this:
            -> (For MP3 and OPUS) Merge tracks' zones
                -> t1 ...|.....|.... <- input zones for track 1
                -> t2 ......|.....|. <- input zones for track 2
                -> t0 ...|........|. <- merge them into zones that will apply on both tracks
            -> Resample the audio to output format (offloaded to audio_utils)
            -> Encrypt zones (MP3 and OPUS - frames) that hold data within start and end times
                of zones (so both tracks get encrypted)
                -> Note: the encryption happens to those frames, who's samples are within
                    a full frame, rather then covering a frame chunk.
                    This means that there is posibility to encrypt lesser zones of those
                    expected by the user.
            -> Write everything to outBuf, including header frames for format
            -> Generate metadata frame for decryption process
            -> Write decryption frame to the outBuf
        */
        std::vector<Zones> merged = mergeAllTrackZones(tracks);
        if (merged.empty())
            return false;
        if (outFormat == AUDIO_MP3)
        {
            if (AudioUtils::ResampleAudio(filepath, 0, outBuf) == 0)
                return false;
        }
        else if (outFormat == AUDIO_OPUS)
        {
            if (AudioUtils::ResampleAudio(filepath, 1, outBuf) == 0)
                return false;
        }
        else
        {
            // Can't work with this format
            return false;
        }

        // Initialise
        randombytes_buf(salt, sizeof(salt));
        SetupKey(pass);
        randombytes_buf(nonce, sizeof(nonce));
        SetupPacketChain();

        // Encrypt with FFMPEG + libsodium
        std::vector<uint8_t> unencryptedBuf = std::move(outBuf);
        outBuf.clear();

        struct BufferReadData
        {
            const std::vector<uint8_t> *buffer;
            size_t offset;
        };
        BufferReadData read_data{&unencryptedBuf, 0};

        const int read_io_buf_size = 32768;
        uint8_t *read_io_buf = static_cast<uint8_t *>(av_malloc(read_io_buf_size));
        AVIOContext *avio_read_ctx = avio_alloc_context(
            read_io_buf, read_io_buf_size, 0, &read_data,
            [](void *opaque, uint8_t *buf, int buf_size) -> int
            {
                auto *data = reinterpret_cast<BufferReadData *>(opaque);
                if (data->offset >= data->buffer->size())
                    return AVERROR_EOF;
                size_t bytes_to_copy = (std::min)(static_cast<size_t>(buf_size), data->buffer->size() - data->offset);
                std::memcpy(buf, data->buffer->data() + data->offset, bytes_to_copy);
                data->offset += bytes_to_copy;
                return static_cast<int>(bytes_to_copy);
            },
            nullptr, nullptr);

        AVFormatContext *ifmt_ctx = avformat_alloc_context();
        ifmt_ctx->pb = avio_read_ctx;

        if (avformat_open_input(&ifmt_ctx, nullptr, nullptr, nullptr) < 0)
        {
            avio_context_free(&avio_read_ctx);
            return false;
        }
        if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
        {
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        int audio_stream_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_stream_idx < 0)
        {
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        EncryptBufferWriteData write_data{&outBuf};
        const int write_io_buf_size = 32768;
        uint8_t *write_io_buf = static_cast<uint8_t *>(av_malloc(write_io_buf_size));

        AVIOContext *avio_write_ctx = avio_alloc_context(
            write_io_buf, write_io_buf_size, 1, &write_data,
            nullptr, encrypt_memory_write_packet, nullptr);

        AVFormatContext *ofmt_ctx = nullptr;
        std::string mux_format = (outFormat == AUDIO_MP3) ? "mp3" : "ogg";
        avformat_alloc_output_context2(&ofmt_ctx, nullptr, mux_format.c_str(), nullptr);
        ofmt_ctx->pb = avio_write_ctx;

        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++)
        {
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
            avcodec_parameters_copy(out_stream->codecpar, ifmt_ctx->streams[i]->codecpar);
            out_stream->time_base = ifmt_ctx->streams[i]->time_base;
        }

        if (avformat_write_header(ofmt_ctx, nullptr) < 0)
        {
            avformat_free_context(ofmt_ctx);
            avio_context_free(&avio_write_ctx);
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        AVStream *in_audio_stream = ifmt_ctx->streams[audio_stream_idx];
        int sample_rate = in_audio_stream->codecpar->sample_rate > 0 ? in_audio_stream->codecpar->sample_rate : 44100;

        // Packet Processing Loop
        AVPacket *pkt = av_packet_alloc();
        bool success = true;

        const AVRational timeBaseUs{1, 1000000};
        int64_t fallbackDurationUs = av_rescale_q(1152, AVRational{1, sample_rate}, timeBaseUs);
        std::vector<uint8_t> macBuf;

        uint64_t pktIdx = 0;
        uint64_t cryptoPktCount = 0;
        uint64_t cryptoByteCount = 0;
        int64_t firstCryptoPkt = -1;
        int64_t lastCryptoPkt = -1;

        while (av_read_frame(ifmt_ctx, pkt) >= 0)
        {
            if (pkt->size > 0)
            {
                AVRational streamTb = ifmt_ctx->streams[pkt->stream_index]->time_base;

                int64_t pkt_start_us = (pkt->pts != AV_NOPTS_VALUE) ? av_rescale_q(pkt->pts, streamTb, timeBaseUs) : 0;
                int64_t pkt_duration_us = (pkt->duration > 0) ? av_rescale_q(pkt->duration, streamTb, timeBaseUs) : fallbackDurationUs;
                int64_t pkt_end_us = pkt_start_us + pkt_duration_us;

                bool should_encrypt = false;

                // Compare packet and zone time intervals [ms]
                for (const auto &zone : merged)
                {
                    if (pkt_end_us >= static_cast<int64_t>(zone.start) &&
                        pkt_start_us <= static_cast<int64_t>(zone.end))
                    {
                        should_encrypt = true;
                        break;
                    }
                }

                if (should_encrypt && av_packet_make_writable(pkt) == 0)
                {
                    size_t headerSkip = GetAudioPacketHeaderSkip(outFormat, pkt->data, pkt->size);
                    if (static_cast<size_t>(pkt->size) > headerSkip)
                    {
                        std::vector<uint8_t> payload(pkt->data + headerSkip, pkt->data + pkt->size - headerSkip);
                        Encrypt(payload, macBuf); // TODO: Provide error checking
                        std::memcpy(pkt->data + headerSkip, payload.data(), payload.size());
                        if (firstCryptoPkt == -1)
                            firstCryptoPkt = static_cast<int64_t>(pktIdx);
                        lastCryptoPkt = static_cast<int64_t>(pktIdx);
                        cryptoPktCount++;
                        cryptoByteCount += payload.size();
                    }
                }
            }

            pktIdx++;
            av_interleaved_write_frame(ofmt_ctx, pkt);
            av_packet_unref(pkt);
        }

        if (success)
        {
            crypto_onetimeauth_poly1305_final(&polyState, macBuf.data());
            sodium_memzero(keystreamBlock, sizeof(keystreamBlock));
            av_write_trailer(ofmt_ctx);
        }

        // Cleanup Resources
        av_packet_free(&pkt);
        avformat_free_context(ofmt_ctx);
        avio_context_free(&avio_write_ctx);
        avformat_close_input(&ifmt_ctx);
        avio_context_free(&avio_read_ctx);

        // Add Metadata
        std::vector<TrackZones> t;
        t.push_back({0, merged});
        GenerateMetadataFrame(outFormat, t, macBuf, outBuf);

        return success;
    }

    bool Encryptor::DoDecrypt(const std::string &filepath, const std::string &pass, Format inFormat, std::vector<uint8_t> &outBuf)
    {
        // Read the encrypted file from disk into a raw byte buffer
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> inBuf(size);
        if (!file.read(reinterpret_cast<char *>(inBuf.data()), size))
            return false;
        file.close();

        // Read and parse the metadata frame from the tail of the file
        EncDecMetadata metadata;
        if (!ReadMetadataFrame(inFormat, inBuf, metadata))
            return false;

        // Restore cryptographic
        if (metadata.salt.size() != sizeof(salt) || metadata.nonce.size() != sizeof(nonce))
            return false;
        std::memcpy(salt, metadata.salt.data(), sizeof(salt));
        std::memcpy(nonce, metadata.nonce.data(), sizeof(nonce));
        if (!SetupKey(pass))
            return false;
        SetupPacketChain();

        const size_t markerLen = std::strlen(kMetaStartMarker);
        const size_t magicLen = 4;
        size_t magicPos = inBuf.size() - magicLen;
        size_t markerPos = magicPos;
        for (size_t i = magicPos; i > 0; --i)
        {
            size_t candidate = i - 1;
            if (candidate + markerLen <= magicPos &&
                std::memcmp(inBuf.data() + candidate, kMetaStartMarker, markerLen) == 0)
            {
                markerPos = candidate;
                break;
            }
        }
        inBuf.resize(markerPos);

        // Setup Memory Read AVIOContext for the stripped audio buffer
        struct BufferReadData
        {
            const std::vector<uint8_t> *buffer;
            size_t offset;
        };
        BufferReadData read_data{&inBuf, 0};

        const int read_io_buf_size = 32768;
        uint8_t *read_io_buf = static_cast<uint8_t *>(av_malloc(read_io_buf_size));
        AVIOContext *avio_read_ctx = avio_alloc_context(
            read_io_buf, read_io_buf_size, 0, &read_data,
            [](void *opaque, uint8_t *buf, int buf_size) -> int
            {
                auto *data = reinterpret_cast<BufferReadData *>(opaque);
                if (data->offset >= data->buffer->size())
                    return AVERROR_EOF;
                size_t bytes_to_copy = (std::min)(static_cast<size_t>(buf_size), data->buffer->size() - data->offset);
                std::memcpy(buf, data->buffer->data() + data->offset, bytes_to_copy);
                data->offset += bytes_to_copy;
                return static_cast<int>(bytes_to_copy);
            },
            nullptr, nullptr);

        AVFormatContext *ifmt_ctx = avformat_alloc_context();
        ifmt_ctx->pb = avio_read_ctx;

        if (avformat_open_input(&ifmt_ctx, nullptr, nullptr, nullptr) < 0)
        {
            avio_context_free(&avio_read_ctx);
            return false;
        }
        if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
        {
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        int audio_stream_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_stream_idx < 0)
        {
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        // Setup Memory Write AVIOContext for output decrypted bytes
        outBuf.clear();
        EncryptBufferWriteData write_data{&outBuf};
        const int write_io_buf_size = 32768;
        uint8_t *write_io_buf = static_cast<uint8_t *>(av_malloc(write_io_buf_size));

        AVIOContext *avio_write_ctx = avio_alloc_context(
            write_io_buf, write_io_buf_size, 1, &write_data,
            nullptr, encrypt_memory_write_packet, nullptr);

        std::string mux_format = (inFormat == AUDIO_MP3) ? "mp3" : "ogg";
        AVFormatContext *ofmt_ctx = nullptr;
        if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, mux_format.c_str(), nullptr) < 0)
        {
            avio_context_free(&avio_write_ctx);
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }
        ofmt_ctx->pb = avio_write_ctx;

        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++)
        {
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
            avcodec_parameters_copy(out_stream->codecpar, ifmt_ctx->streams[i]->codecpar);
            out_stream->time_base = ifmt_ctx->streams[i]->time_base;
        }

        if (avformat_write_header(ofmt_ctx, nullptr) < 0)
        {
            avformat_free_context(ofmt_ctx);
            avio_context_free(&avio_write_ctx);
            avformat_close_input(&ifmt_ctx);
            avio_context_free(&avio_read_ctx);
            return false;
        }

        AVStream *in_audio_stream = ifmt_ctx->streams[audio_stream_idx];
        int sample_rate = in_audio_stream->codecpar->sample_rate > 0 ? in_audio_stream->codecpar->sample_rate : 44100;
        const AVRational timeBaseUs{1, 1000000};
        int64_t fallbackDurationUs = av_rescale_q(1152, AVRational{1, sample_rate}, timeBaseUs);

        // Flatten zones for efficient lookup
        std::vector<ModificationMethod::Zones> flatZones;
        for (const auto &tZone : metadata.trackZones)
            for (const auto &z : tZone.track)
                flatZones.push_back(z);

        // Packet Processing & Decryption Loop
        AVPacket *pkt = av_packet_alloc();
        bool success = true;
        std::vector<uint8_t> macBuf = metadata.mac;

        uint64_t pktIdx = 0;
        uint64_t cryptoPktCount = 0;
        uint64_t cryptoByteCount = 0;
        int64_t firstCryptoPkt = -1;
        int64_t lastCryptoPkt = -1;

        while (av_read_frame(ifmt_ctx, pkt) >= 0)
        {
            if (pkt->size > 0)
            {
                AVRational streamTb = ifmt_ctx->streams[pkt->stream_index]->time_base;
                int64_t pkt_start_us = (pkt->pts != AV_NOPTS_VALUE) ? av_rescale_q(pkt->pts, streamTb, timeBaseUs) : 0;
                int64_t pkt_duration_us = (pkt->duration > 0) ? av_rescale_q(pkt->duration, streamTb, timeBaseUs) : fallbackDurationUs;
                int64_t pkt_end_us = pkt_start_us + pkt_duration_us;

                bool should_decrypt = false;
                for (const auto &zone : flatZones)
                {
                    if (pkt_end_us >= static_cast<int64_t>(zone.start) &&
                        pkt_start_us <= static_cast<int64_t>(zone.end))
                    {
                        should_decrypt = true;
                        break;
                    }
                }

                if (should_decrypt && av_packet_make_writable(pkt) == 0)
                {
                    size_t headerSkip = GetAudioPacketHeaderSkip(inFormat, pkt->data, pkt->size);
                    if (static_cast<size_t>(pkt->size) > headerSkip)
                    {
                        std::vector<uint8_t> payload(pkt->data + headerSkip, pkt->data + pkt->size - headerSkip);
                        Decrypt(payload, macBuf); // TODO: Provide error checking
                        std::memcpy(pkt->data + headerSkip, payload.data(), payload.size());
                        if (firstCryptoPkt == -1)
                            firstCryptoPkt = static_cast<int64_t>(pktIdx);
                        lastCryptoPkt = static_cast<int64_t>(pktIdx);
                        cryptoPktCount++;
                        cryptoByteCount += payload.size();
                    }
                }
            }

            pktIdx++;
            av_interleaved_write_frame(ofmt_ctx, pkt);
            av_packet_unref(pkt);
        }

        if (success)
        {
            av_write_trailer(ofmt_ctx);
            if (metadata.mac.size() != crypto_onetimeauth_poly1305_BYTES)
            {
                sodium_memzero(keystreamBlock, sizeof(keystreamBlock));
                outBuf.clear();
                success = false;
            }
            unsigned char computedMac[crypto_onetimeauth_poly1305_BYTES];
            crypto_onetimeauth_poly1305_final(&polyState, computedMac);
            sodium_memzero(keystreamBlock, sizeof(keystreamBlock));

            success = (sodium_memcmp(computedMac, metadata.mac.data(), crypto_onetimeauth_poly1305_BYTES) == 0);
            sodium_memzero(computedMac, sizeof(computedMac));
        }

        // Cleanup
        av_packet_free(&pkt);
        avformat_free_context(ofmt_ctx);
        avio_context_free(&avio_write_ctx);
        avformat_close_input(&ifmt_ctx);
        avio_context_free(&avio_read_ctx);

        return success;
    }

    bool Encryptor::Encrypt(std::vector<uint8_t> &inOutBuf, std::vector<uint8_t> &outMacBuf)
    {
        if (inOutBuf.empty() || !isInit)
            return false;

        if (outMacBuf.size() != crypto_onetimeauth_poly1305_BYTES)
            outMacBuf.resize(crypto_onetimeauth_poly1305_BYTES);

        size_t i = 0;
        size_t len = inOutBuf.size();
        while (i < len)
        {
            if (offsetInBlock == 64)
            {
                std::memset(keystreamBlock, 0, sizeof(keystreamBlock));
                crypto_stream_chacha20_ietf_xor_ic(
                    keystreamBlock,
                    keystreamBlock,
                    sizeof(keystreamBlock),
                    nonce,
                    blockCounter++,
                    key);
                offsetInBlock = 0;
            }

            size_t chunk = (std::min)(len - i, 64 - offsetInBlock);
            for (size_t j = 0; j < chunk; ++j)
                inOutBuf.data()[i + j] ^= keystreamBlock[offsetInBlock + j];
            i += chunk;
            offsetInBlock += chunk;
        }

        if (crypto_onetimeauth_poly1305_update(&polyState, inOutBuf.data(), inOutBuf.size()) != 0)
            return false;

        return true;
    }

    bool Encryptor::Decrypt(std::vector<uint8_t> &inOutBuf, std::vector<uint8_t> &inMacBuf)
    {
        if (inOutBuf.empty() || !isInit)
            return false;

        if (inMacBuf.size() != crypto_onetimeauth_poly1305_BYTES)
            return false;

        if (crypto_onetimeauth_poly1305_update(&polyState, inOutBuf.data(), inOutBuf.size()) != 0)
            return false;

        size_t i = 0;
        size_t len = inOutBuf.size();
        while (i < len)
        {
            if (offsetInBlock == 64)
            {
                std::memset(keystreamBlock, 0, sizeof(keystreamBlock));
                crypto_stream_chacha20_ietf_xor_ic(
                    keystreamBlock,
                    keystreamBlock,
                    sizeof(keystreamBlock),
                    nonce,
                    blockCounter++,
                    key);
                offsetInBlock = 0;
            }

            size_t chunk = (std::min)(len - i, 64 - offsetInBlock);
            for (size_t j = 0; j < chunk; ++j)
                inOutBuf.data()[i + j] ^= keystreamBlock[offsetInBlock + j];
            i += chunk;
            offsetInBlock += chunk;
        }

        return true;
    }

    bool Encryptor::SetupKey(const std::string &pass)
    {
        if (crypto_pwhash(key, sizeof(key),
                          pass.data(), pass.size(), salt, opslimit,
                          memlimit, alg) < 0)
        {
            // Not enough mem to perform operation
            return false;
        }

        return true;
    }

    bool Encryptor::SetSalt(unsigned char *s, size_t cap)
    {
        if (cap != crypto_pwhash_SALTBYTES)
            return false;

        memcpy(salt, s, cap);
        return true;
    }

    bool Encryptor::GenerateMetadataFrame(Format outFormat, const std::vector<ModificationMethod::TrackZones> &tracks, std::vector<uint8_t> &inMacBuf, std::vector<uint8_t> &outBuf)
    {
        if (outBuf.empty())
            return false;

        // Helper to convert binary buffers to Hex strings for text-safe metadata storage
        auto toHex = [](const unsigned char *data, size_t len)
        {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            for (size_t i = 0; i < len; ++i)
                ss << std::setw(2) << static_cast<int>(data[i]);
            return ss.str();
        };

        std::string saltHex = toHex(salt, sizeof(salt));
        std::string nonceHex = toHex(nonce, sizeof(nonce));
        std::string macHex = toHex(inMacBuf.data(), sizeof(inMacBuf.data()) * 2);

        std::string zonesStr = "";
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            if (t > 0)
            {
                zonesStr += "|";
            }
            zonesStr += std::to_string(tracks[t].track_id);
            for (const auto &zone : tracks[t].track)
            {
                zonesStr += ":" + std::to_string(zone.start) + ":" + std::to_string(zone.end);
            }
        }

        std::string metaLine = saltHex + ";" + nonceHex + ";" + macHex + ";" + zonesStr;

        // Append: [start marker][metadata payload][end magic "AUDE"]
        outBuf.insert(outBuf.end(), kMetaStartMarker, kMetaStartMarker + std::strlen(kMetaStartMarker));
        outBuf.insert(outBuf.end(), metaLine.begin(), metaLine.end());

        const uint8_t magic[4] = {'A', 'U', 'D', 'E'};
        outBuf.insert(outBuf.end(), magic, magic + 4);

        return true;
    }

    bool Encryptor::ReadMetadataFrame(Format inFormat, std::vector<uint8_t> &inBuf, EncDecMetadata &outMetadata)
    {
        const size_t markerLen = std::strlen(kMetaStartMarker);
        const size_t magicLen = 4;
        if (inBuf.size() < markerLen + magicLen)
            return false;

        // Verify trailing "AUDE" magic bytes
        size_t magicPos = inBuf.size() - magicLen;
        if (inBuf[magicPos] != 'A' || inBuf[magicPos + 1] != 'U' ||
            inBuf[magicPos + 2] != 'D' || inBuf[magicPos + 3] != 'E')
        {
            return false;
        }

        // Search backwards (up to kMaxMetadataBytes) for the start marker
        size_t searchStart = (magicPos > kMaxMetadataBytes) ? (magicPos - kMaxMetadataBytes) : 0;
        bool foundMarker = false;
        size_t markerPos = 0;

        for (size_t i = magicPos; i > searchStart; --i)
        {
            size_t candidate = i - 1;
            if (candidate + markerLen <= magicPos &&
                std::memcmp(inBuf.data() + candidate, kMetaStartMarker, markerLen) == 0)
            {
                markerPos = candidate;
                foundMarker = true;
                break;
            }
        }

        if (!foundMarker)
            return false;

        // Extract metadata string between marker and "AUDE"
        size_t payloadStart = markerPos + markerLen;
        size_t payloadLen = magicPos - payloadStart;
        std::string metaLine(reinterpret_cast<const char *>(inBuf.data() + payloadStart), payloadLen);

        auto fromHex = [](const std::string &hex, std::vector<uint8_t> &out) -> bool
        {
            if (hex.empty() || (hex.size() % 2) != 0)
                return false;
            out.clear();
            out.reserve(hex.size() / 2);
            try
            {
                for (size_t i = 0; i < hex.size(); i += 2)
                {
                    size_t parsedChars = 0;
                    int val = std::stoi(hex.substr(i, 2), &parsedChars, 16);
                    if (parsedChars != 2 || val < 0 || val > 255)
                        return false;
                    out.push_back(static_cast<uint8_t>(val));
                }
            }
            catch (...)
            {
                return false;
            }
            return true;
        };

        // Parse fields: saltHex;nonceHex;macHex;zonesStr
        std::stringstream ss(metaLine);
        std::string saltHex, nonceHex, macHex, zonesPart;

        if (!std::getline(ss, saltHex, ';') ||
            !std::getline(ss, nonceHex, ';') ||
            !std::getline(ss, macHex, ';') ||
            !std::getline(ss, zonesPart, ';'))
        {
            return false;
        }

        if (!fromHex(saltHex, outMetadata.salt) ||
            !fromHex(nonceHex, outMetadata.nonce) ||
            !fromHex(macHex, outMetadata.mac))
        {
            return false;
        }

        // Parse zones sequence: track_id:start:end,...
        outMetadata.trackZones.clear();

        if (!zonesPart.empty())
        {
            std::stringstream trackStream(zonesPart);
            std::string trackBlock;

            while (std::getline(trackStream, trackBlock, '|'))
            {
                if (trackBlock.empty())
                    return false;

                std::vector<std::string> tokens;
                std::stringstream tokenStream(trackBlock);
                std::string tok;
                while (std::getline(tokenStream, tok, ':'))
                    tokens.push_back(tok);

                // Must have at least track_id plus one (start, end) pair, and an odd total count:
                // [trackId, start1, end1, start2, end2, ...] -> 1 + 2*N tokens
                if (tokens.size() < 3 || (tokens.size() - 1) % 2 != 0)
                    return false;

                uint64_t trackId = 0;
                try
                {
                    size_t parsedLen = 0;
                    trackId = std::stoi(tokens[0], &parsedLen);
                    if (parsedLen != tokens[0].size() || trackId < 0)
                        return false;
                }
                catch (...)
                {
                    return false;
                }

                std::vector<ModificationMethod::Zones> parsedZones;
                parsedZones.reserve((tokens.size() - 1) / 2);

                for (size_t i = 1; i + 1 < tokens.size(); i += 2)
                {
                    uint64_t startUs = 0;
                    uint64_t endUs = 0;
                    try
                    {
                        size_t startLen = 0;
                        size_t endLen = 0;
                        startUs = std::stoull(tokens[i], &startLen);
                        endUs = std::stoull(tokens[i + 1], &endLen);

                        if (startLen != tokens[i].size() || endLen != tokens[i + 1].size())
                            return false;
                    }
                    catch (...)
                    {
                        return false;
                    }

                    if (startUs > endUs)
                        return false;

                    parsedZones.push_back(ModificationMethod::Zones{startUs, endUs});
                }

                outMetadata.trackZones.push_back(
                    ModificationMethod::TrackZones{trackId, std::move(parsedZones)});
            }
        }

        return true;
    }

    std::vector<Zones> Encryptor::mergeAllTrackZones(const std::vector<TrackZones> &tracks)
    {
        std::vector<Zones> all_zones;
        for (const auto &tz : tracks)
            all_zones.insert(all_zones.end(), tz.track.begin(), tz.track.end());
        if (all_zones.empty())
            return {};

        std::sort(all_zones.begin(), all_zones.end(), [](const Zones &a, const Zones &b)
                  { return a.start < b.start; });

        std::vector<Zones> merged;
        merged.push_back(all_zones[0]);

        for (size_t i = 1; i < all_zones.size(); ++i)
        {
            Zones &last = merged.back();
            const Zones &current = all_zones[i];

            if (current.start <= last.end + 1)
                last.end = (std::max)(last.end, current.end);
            else
                merged.push_back(current);
        }

        return merged;
    }

    bool Encryptor::SetupPacketChain()
    {
        // Derive the 32-byte one-time Poly1305 key from ChaCha20 block 0 (ic = 0)
        uint8_t block0[64]{};
        if (crypto_stream_chacha20_ietf(block0, sizeof(block0), nonce, key) != 0)
        {
            return false;
        }

        if (crypto_onetimeauth_poly1305_init(&polyState, block0) != 0)
        {
            return false;
        }
        sodium_memzero(block0, sizeof(block0));

        // Reset continuous keystream cursor to block 1 (ic = 1)
        blockCounter = 1;
        offsetInBlock = 64;
        sodium_memzero(keystreamBlock, sizeof(keystreamBlock));

        return true;
    }
}
