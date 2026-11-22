#ifndef GUI_HPP
#define GUI_HPP

#include <vector>
#include <memory>
#include <filesystem>
#include <thread>
#include <fstream>
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/slider.h>
#include <wx/choice.h>
#include <wx/scrolwin.h>
#include <wx/checkbox.h>
#include <wx/dnd.h>
#include "../utils/config.hpp"
#include "../utils/text_utils.hpp"
#include "../utils/audio_utils.hpp"
#include "../utils/audio_player.hpp"
#include "../methods/transcribe/transcription.hpp"
#include "../methods/modify/modify.hpp"

// Declare the application class
class AudioApp : public wxApp
{
public:
    // Called on application startup
    virtual bool OnInit();
};

class MainFrame;

// Structure to hold a selection zone (in pixels)
struct TrackZone
{
    int start;
    int end;
};

// Structure to hold individual track state
struct TrackData
{
    std::vector<float> wave;
    std::vector<TrackZone> zones;
    bool isMuted = false;
};

// Solid scrollable window for tracks
class MultiTrackWaveformCtrl : public wxScrolledWindow
{
public:
    // Constructor
    MultiTrackWaveformCtrl(wxWindow *parent, MainFrame *owner, wxWindowID id = wxID_ANY);

    // Event Handlers
    void OnPaint(wxPaintEvent &event);
    void OnLeftDown(wxMouseEvent &event);
    void OnLeftUp(wxMouseEvent &event);
    void OnRightDown(wxMouseEvent &event);
    void OnRightUp(wxMouseEvent &event);
    void OnMouseMove(wxMouseEvent &event);
    void OnSize(wxSizeEvent &event);
    void OnScroll(wxScrollWinEvent &event);

    // Other Handlers
    void LoadTracks(const wxString &filepath);
    void ClearTracks();
    void UpdateVirtualSize();
    void MergeZones(int trackIdx);
    void ClearZones();
    void SetSeekPos(int pos);
    int GetSeekPosition() const { return m_seekPos; }
    int GetLogicalWidth() const;
    std::vector<TrackData> GetTracks() const { return m_tracks; }
    size_t GetTotalSamples() const { return m_totalSamples; }
    size_t GetSampleRate() const { return m_sampleRate; }

private:
    MainFrame *m_owner;
    std::vector<TrackData> m_tracks;

    // Unified seek position
    int m_seekPos = 0;
    size_t m_totalSamples = 0;
    int m_sampleRate = 44100;

    // Selection state
    int m_dragStart = -1;
    int m_dragEnd = -1;
    int m_dragTrackIdx = -1;
    bool m_isSelecting = false;

    // Dimensions
    const int HEADER_WIDTH = 80;
    int m_trackHeight = 100;

    DECLARE_EVENT_TABLE();
};

class SettingsDialog : public wxDialog
{
public:
    SettingsDialog(wxWindow *parent);
    void BuildUI();
    std::string GetModelPath();
    int GetModelSelection();
    bool GetUseGPU();
    std::string GetKeywords();

private:
    wxStaticText *m_modelPathLabel = new wxStaticText;
    wxTextCtrl *m_modelPath = new wxTextCtrl;
    wxArrayString m_modelOptions = {
        wxString::FromUTF8("Whisper.cpp - Tiny"),
        wxString::FromUTF8("Whisper.cpp - Base"),
        wxString::FromUTF8("Whisper.cpp - Small"),
        wxString::FromUTF8("Whisper.cpp - Medium"),
        wxString::FromUTF8("Whisper.cpp - Large v2"),
        wxString::FromUTF8("Whisper.cpp - Large v3"),
        wxString::FromUTF8("Whisper.cpp - Large v3 turbo"),
    };
    wxStaticText *m_modelOptionLabel = new wxStaticText;
    wxChoice *m_modelSelection = new wxChoice;
    wxCheckBox *m_useGPU = new wxCheckBox;
    wxStaticText *m_keywordsLabel = new wxStaticText;
    wxTextCtrl *m_keywords = new wxTextCtrl;

    wxBoxSizer *m_mainSizer = new wxBoxSizer(wxVERTICAL);
    // wxFlexGridSizer: 2 columns, 10px vertical gap, 15px horizontal gap
    wxFlexGridSizer *m_grid = new wxFlexGridSizer(2, 10, 15);
    wxSizer *m_btnSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
};

// Declare our main frame class
class MainFrame : public wxFrame
{
public:
    // Constructor
    MainFrame(const wxString &title);
    ~MainFrame();

    // Event Handlers
    void OnOpen(wxCommandEvent &event);
    void OnRemove(wxCommandEvent &event);
    void OnSettings(wxCommandEvent &event);
    void OnTranscribe(wxCommandEvent &event);
    void OnModify(wxCommandEvent &event);
    void OnFileSelected(wxCommandEvent &event);
    void OnColorModeChanged(wxCommandEvent &event);
    void OnTranscriptionTextChanged(wxCommandEvent &event);
    void OnPlay(wxCommandEvent &event);
    void OnPause(wxCommandEvent &event);
    void OnSeekBegin(wxCommandEvent &event);
    void OnSeekEnd(wxCommandEvent &event);
    void OnResetZones(wxCommandEvent &event);
    void OnVolumeChanged(wxCommandEvent &event);

    // Other Handlers
    void AddFile(const wxString &path);
    void UpdateState();
    inline void BuildRightPanel();
    void ApplyTextColors();
    void LoadDebugText();
    void SeekAudioFromWaveform(size_t frameIndex);
    std::string GetModelPath() { return m_pathModel; }
    TranscribeMethod::Model GetSelectedModel() { return m_selectedModel; }
    bool GetUseGPU() { return m_useGPU; }
    std::string GetKeywords() { return m_textBank->GetKeywords(); }

private:
    // Main UI
    std::unique_ptr<wxPanel> m_mainPanel = std::make_unique<wxPanel>();
    // wxPanel* m_mainPanel = new wxPanel;
    wxBoxSizer *m_mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left side UI
    wxPanel *m_leftPanel = new wxPanel;
    wxListBox *m_fileList = new wxListBox;
    wxButton *m_btnOpen = new wxButton;
    wxButton *m_btnRemove = new wxButton;
    wxButton *m_btnSettings = new wxButton;
    wxButton *m_btnTranscribe = new wxButton;
    wxButton *m_btnEncrypt = new wxButton;
    wxBoxSizer *leftSizer = new wxBoxSizer(wxVERTICAL);      // General Sizer
    wxBoxSizer *fileBtnSizer = new wxBoxSizer(wxHORIZONTAL); // Open-Close files Sizer

    // Right side UI (Expanded uppon file load)
    wxPanel *m_rightPanel = new wxPanel;
    MultiTrackWaveformCtrl *m_multiTrackCtrl;
    wxButton *m_btnPlay = new wxButton;
    wxButton *m_btnPause = new wxButton;
    wxButton *m_btnSeekBegin = new wxButton;
    wxButton *m_btnSeekEnd = new wxButton;
    wxButton *m_btnReset = new wxButton;
    wxSlider *m_sldVolume = new wxSlider;
    wxStaticText *m_audioTextHeader = new wxStaticText;
    wxArrayString m_audioTextChoices = {
        wxString::FromUTF8("Впевненість"),   // AI-model transcription confidence
        wxString::FromUTF8("Ключові слова"), // Key-word indicators of potential secret leaks
        wxString::FromUTF8("Звичайний")      // Plain text - Make sure to NOT move it from the bottom
    };
    wxChoice *m_audioTextHighligh = new wxChoice;
    wxTextCtrl *m_audioText = new wxTextCtrl;
    wxStaticText *m_outputFormatHeader = new wxStaticText;
    wxArrayString m_formats = {wxString::FromUTF8("MP3"), wxString::FromUTF8("Opus")};
    wxChoice *m_outputFormatChoice = new wxChoice;
    wxCheckBox *m_decryptChoice = new wxCheckBox;
    wxStaticText *m_passwordHeader = new wxStaticText;
    wxTextCtrl *m_passwordText = new wxTextCtrl;
    wxBoxSizer *m_rightSizer = new wxBoxSizer(wxVERTICAL);   // General Sizer
    wxBoxSizer *m_trackSizer = new wxBoxSizer(wxVERTICAL);   // Sizer for audio tracks
    wxBoxSizer *m_ctrlSizer = new wxBoxSizer(wxHORIZONTAL);  // Sizer for controls
    wxBoxSizer *m_textHSizer = new wxBoxSizer(wxHORIZONTAL); // Sizer for text field's headers
    wxBoxSizer *m_modSizer = new wxBoxSizer(wxHORIZONTAL);   // Sizer for encryption

    const int m_baseWidth = window_width_init;
    const int m_baseHeight = window_height_init;
    const int m_expandedWidth = window_width_expanded;
    bool m_isExpanded = false;

    AudioPlayer m_audioPlayer;

    TextUtils::TextBank *m_textBank = new TextUtils::TextBank();
    bool m_isUpdatingText = false;

    std::string m_pathModel = std::filesystem::current_path().string() + "\\models\\ggml-base.bin";
    TranscribeMethod::Model m_selectedModel = {TranscribeMethod::Models::GGML_WHISPER_BASE, m_pathModel};
    bool m_useGPU = 1;
    TranscribeMethod::Transcriber *m_transcriber = new TranscribeMethod::Transcriber(m_selectedModel, m_useGPU);
    std::thread m_trThread;
    std::thread m_enThread;

    ModificationMethod::Encryptor *m_encryptor = new ModificationMethod::Encryptor();

    DECLARE_EVENT_TABLE();
};

// Drag and drop support for the file list
// Needs a Drop Target bound to the MainFrame class and set to a ListBox-like object
class FileDropTarget : public wxFileDropTarget
{
public:
    FileDropTarget(MainFrame *owner);
    virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames) override;

private:
    MainFrame *m_owner;
};

#endif // GUI_HPP
