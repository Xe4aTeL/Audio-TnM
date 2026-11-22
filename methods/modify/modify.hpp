#ifndef MODIFY_HPP
#define MODIFY_HPP

#include <sodium.h>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "../../utils/audio_utils.hpp"

namespace ModificationMethod
{
    /*
        Needs ChaCha20
        Poly1305

        Zones to enrypt
        Password
        Final format

        FFMPEG -> Resample (MP3 or Opus)
        -> Modify frames
        -> Push Metadata to a file
        -> return std::stream for file output
        -> MainFrame-->ask where to write-->write stream

        Needs to be able to:
        + Init ChaCha20
        + Encrypt
        - Write Metadata
        - Find and Validate Metadata
            ^ Might want to try and read it on the file open
        + Decrypt

        You can only enrypt or decrypt (without ruining the file) once.
    */

    enum Format
    {
        AUDIO_MP3,
        AUDIO_OPUS
    };

    /*
        Unlike in GUI, this zones expect to have a starting sample number
        and the ending sample number, rather than a pixel on a screen.
        The zone will produce frame numbers to be encrypted or decrypted,
        but tbh I forgor whether I wanted to store samples or frames in
        a metadata.
        UPD: I've decided to go for sample number hereю
        UPD2: Actually, screw it, we ball (it's now a ms timestamp)
    */
    struct Zones
    {
        uint64_t start;
        uint64_t end;
    };

    struct TrackZones
    {
        uint64_t track_id;
        std::vector<Zones> track;
    };

    struct EncDecMetadata
    {
        std::vector<uint8_t> salt = std::vector<uint8_t>(crypto_pwhash_SALTBYTES);
        std::vector<uint8_t> nonce = std::vector<uint8_t>(crypto_aead_chacha20poly1305_ietf_NPUBBYTES);
        std::vector<uint8_t> mac = std::vector<uint8_t>(crypto_onetimeauth_poly1305_BYTES);
        std::vector<ModificationMethod::TrackZones> trackZones;
    };

    class Encryptor
    {
    public:
        Encryptor()
        {
            if (sodium_init() >= 0)
                isInit = true;
        };
        // This 2 functions are gonna resample + encrypt, unlike Encrypt() and Decrypt()
        bool DoEncrypt(const std::string &filepath, const std::string &pass, Format outFormat, std::vector<TrackZones> tracks, std::vector<uint8_t> &outBuf);
        bool DoDecrypt(const std::string &filepath, const std::string &pass, Format inFormat, std::vector<uint8_t> &outBuf);
        bool SetupKey(const std::string &pass);
        bool ReadMetadataFrame(Format inFormat, std::vector<uint8_t> &inBuf, EncDecMetadata &outMetadata);

    private:
        bool Encrypt(std::vector<uint8_t> &inOutBuf, std::vector<uint8_t> &outMacBuf);
        bool Decrypt(std::vector<uint8_t> &inOutBuf, std::vector<uint8_t> &inMacBuf);
        bool SetupNonce();
        bool SetSalt(unsigned char *s, size_t cap); // For decryption only
        bool GenerateMetadataFrame(Format outFormat, const std::vector<ModificationMethod::TrackZones> &tracks, std::vector<uint8_t> &inMacBuf, std::vector<uint8_t> &outBuf);
        
        std::vector<Zones> mergeAllTrackZones(const std::vector<TrackZones> &tracks);
        bool SetupPacketChain();

        bool isInit = false;
        unsigned char key[crypto_aead_chacha20poly1305_ietf_KEYBYTES];
        unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
        unsigned char salt[crypto_pwhash_SALTBYTES];
        const unsigned long long opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
        const size_t memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
        const int alg = crypto_pwhash_ALG_DEFAULT;
        uint32_t blockCounter;
        uint64_t offsetInBlock;
        uint8_t keystreamBlock[64]{};
        crypto_onetimeauth_poly1305_state polyState;
    };
}

#endif // MODIFY_HPP
