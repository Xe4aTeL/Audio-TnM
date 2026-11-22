#pragma once

// Main window
constexpr int window_width_init = 330;
constexpr int window_height_init = 720;
constexpr int window_width_expanded = 1280;
constexpr char app_name[] = { "Audio Transcribe and Modify tool" };

// Settings window
constexpr int settings_window_width = 450;
constexpr int settings_window_height = 450;

// Events IDs
constexpr int OPENAUDIO = 1001;              // Open Audio File
constexpr int REMOVEAUDIO = 1002;            // Remove Audio File Entry
constexpr int OPENSETTINGS = 1003;           // Open Settings
constexpr int TRANSCRIBEAUDIO = 1004;        // Transcribe Selected File
constexpr int ENCRYPTAUDIO = 1005;           // Encrypt Selected File
constexpr int SELECTAUDIO = 2001;            // Generate and Display Audio Waveform (File Loaded and Selected)
constexpr int SELECTCOLORMODE = 3001;        // Selected Another Color Mode (Combo)
constexpr int TRANSCRIPTIONMOD = 3002;       // Text of Transcription is Modified
constexpr int PLAYAUDIO = 4001;              // Play Audio
constexpr int PAUSEAUDIO = 4002;             // Pause Audio
constexpr int SEEKBEGAUDIO = 4003;           // Seek to the Beginning of an Audio
constexpr int SEEKENDAUDIO = 4004;           // Seek to the End of an Audio
constexpr int CLEARZONES = 4005;             // Clear Selection Zone (RMB)
constexpr int VOLUMECHANGE = 4006;           // Change Volume
constexpr int TRANSCRIPTIONDONE = 5001;      // Transcription
