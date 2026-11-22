#include <wx/dcbuffer.h>
#include <algorithm>
#include "gui.hpp"

// Main Frame Implementation
MainFrame::MainFrame(const wxString &title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(window_width_init, window_height_init))
{
    SetMinSize(wxSize(m_baseWidth, m_baseHeight));
    m_mainPanel->Create(this, wxID_ANY);

    // Left panel
    m_leftPanel->Create(m_mainPanel.get(), wxID_ANY);
    m_leftPanel->SetMinSize(wxSize(m_baseWidth, m_baseHeight));

    // File list up top
    // With horizontal scrolling of long filenames
    // And Drag'n'Drop support
    m_fileList->Create(m_leftPanel, SELECTAUDIO, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_HSCROLL | wxLB_NEEDED_SB);
    m_fileList->SetDropTarget(new FileDropTarget(this));
    leftSizer->Add(m_fileList, 1, wxEXPAND | wxALL, 12);

    // Buttons to open and close files
    m_btnOpen->Create(m_leftPanel, OPENAUDIO, wxString::FromUTF8("Відкрити\nфайл"), wxDefaultPosition, wxSize(-1, 40));
    m_btnRemove->Create(m_leftPanel, REMOVEAUDIO, wxString::FromUTF8("Видалити\nфайл"), wxDefaultPosition, wxSize(-1, 40));
    fileBtnSizer->Add(m_btnOpen, 1, wxLEFT);
    fileBtnSizer->Add(m_btnRemove, 1, wxRIGHT);
    leftSizer->Add(fileBtnSizer, 0, wxEXPAND | wxALL, 10);

    // Control buttons: settings, transcribe, encrypt
    m_btnSettings->Create(m_leftPanel, OPENSETTINGS, wxString::FromUTF8("Налаштування"), wxDefaultPosition, wxSize(-1, 40));
    m_btnTranscribe->Create(m_leftPanel, TRANSCRIBEAUDIO, wxString::FromUTF8("Транскрипція"), wxDefaultPosition, wxSize(-1, 40));
    m_btnEncrypt->Create(m_leftPanel, ENCRYPTAUDIO, wxString::FromUTF8("Модифікація"), wxDefaultPosition, wxSize(-1, 40));
    leftSizer->Add(m_btnSettings, 0, wxEXPAND | wxALL, 10);
    leftSizer->Add(m_btnTranscribe, 0, wxEXPAND | wxALL, 10);
    leftSizer->Add(m_btnEncrypt, 0, wxEXPAND | wxALL, 10);
    m_leftPanel->SetSizer(leftSizer);
    // This thing will not grow with window expantion, but the right panel will
    // I am too tired to think of how to fix it
    m_mainSizer->Add(m_leftPanel, 0, wxEXPAND);

    // Right panel
    BuildRightPanel();

    // Slop everything up
    m_mainSizer->Add(m_rightPanel, 1, wxEXPAND);
    m_rightPanel->Hide(); // Hidden by default, revealed if an audio file is opened
    m_mainPanel->SetSizer(m_mainSizer);

    UpdateState();
}

MainFrame::~MainFrame()
{
    if (m_trThread.joinable())
        m_trThread.join();
    if (m_enThread.joinable())
        m_enThread.join();
}

// Main frame Event Table
wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_BUTTON(OPENAUDIO, MainFrame::OnOpen)
EVT_BUTTON(REMOVEAUDIO, MainFrame::OnRemove)
EVT_BUTTON(OPENSETTINGS, MainFrame::OnSettings)
EVT_BUTTON(TRANSCRIBEAUDIO, MainFrame::OnTranscribe)
EVT_BUTTON(ENCRYPTAUDIO, MainFrame::OnModify)
EVT_LISTBOX(SELECTAUDIO, MainFrame::OnFileSelected)
EVT_CHOICE(SELECTCOLORMODE, MainFrame::OnColorModeChanged)
EVT_TEXT(TRANSCRIPTIONMOD, MainFrame::OnTranscriptionTextChanged)
EVT_BUTTON(PLAYAUDIO, MainFrame::OnPlay)
EVT_BUTTON(PAUSEAUDIO, MainFrame::OnPause)
EVT_BUTTON(SEEKBEGAUDIO, MainFrame::OnSeekBegin)
EVT_BUTTON(SEEKENDAUDIO, MainFrame::OnSeekEnd)
EVT_BUTTON(CLEARZONES, MainFrame::OnResetZones)
EVT_SLIDER(VOLUMECHANGE, MainFrame::OnVolumeChanged)
wxEND_EVENT_TABLE()

FileDropTarget::FileDropTarget(MainFrame *owner) : m_owner(owner)
{
}

bool FileDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames)
{
    for (size_t i = 0; i < filenames.GetCount(); i++)
    {
        wxString ext = filenames[i].AfterLast('.').Lower();
        if (ext == "wav" || ext == "mp3" || ext == "opus")
            m_owner->AddFile(filenames[i]);
    }
    return true;
}

/*
    This RightPanel is built when the user imports first audio file
    or first audio file after clearing the whole list prior to that
*/
inline void MainFrame::BuildRightPanel()
{
    m_rightPanel->Create(m_mainPanel.get(), wxID_ANY);
    m_rightPanel->SetMinSize(wxSize((m_expandedWidth - m_baseWidth), m_baseHeight));

    m_multiTrackCtrl = new MultiTrackWaveformCtrl(m_rightPanel, this);
    m_rightSizer->Add(m_multiTrackCtrl, 1, wxEXPAND | wxALL, 5);

    // Middle Upper: Audio Controls (might rewrite as wxBitmapButton)
    m_btnPlay->Create(m_rightPanel, PLAYAUDIO, wxString::FromUTF8("▶"), wxDefaultPosition, wxSize(60, 45));
    m_btnPause->Create(m_rightPanel, PAUSEAUDIO, wxString::FromUTF8("⏸"), wxDefaultPosition, wxSize(60, 45));
    m_btnSeekBegin->Create(m_rightPanel, SEEKBEGAUDIO, wxString::FromUTF8("⏮"), wxDefaultPosition, wxSize(60, 45));
    m_btnSeekEnd->Create(m_rightPanel, SEEKENDAUDIO, wxString::FromUTF8("⏭"), wxDefaultPosition, wxSize(60, 45));
    m_btnReset->Create(m_rightPanel, CLEARZONES, wxString::FromUTF8("⟳"), wxDefaultPosition, wxSize(60, 45));
    m_sldVolume->Create(m_rightPanel, VOLUMECHANGE, 50, 0, 100, wxDefaultPosition, wxSize(100, 30),
                        wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_LABELS);
    m_btnPlay->SetToolTip(new wxToolTip(wxString::FromUTF8("Програвання")));
    m_btnPause->SetToolTip(new wxToolTip(wxString::FromUTF8("Пауза")));
    m_btnSeekBegin->SetToolTip(new wxToolTip(wxString::FromUTF8("До початку")));
    m_btnSeekEnd->SetToolTip(new wxToolTip(wxString::FromUTF8("До кінця")));
    m_btnReset->SetToolTip(new wxToolTip(wxString::FromUTF8("Скидання виділення")));
    m_sldVolume->SetToolTip(new wxToolTip(wxString::FromUTF8("Гучність")));
    m_ctrlSizer->Add(m_btnSeekBegin, 0, wxRIGHT, 5);
    m_ctrlSizer->Add(m_btnPlay, 0, wxRIGHT, 5);
    m_ctrlSizer->Add(m_btnPause, 0, wxRIGHT, 5);
    m_ctrlSizer->Add(m_btnSeekEnd, 0, wxRIGHT, 5);
    m_ctrlSizer->Add(m_btnReset, 0, wxRIGHT, 5);
    m_ctrlSizer->Add(m_sldVolume, 0, wxLEFT, 5);
    m_rightSizer->Add(m_ctrlSizer, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    // Middle Lower: Transcription Text
    m_audioTextHeader->Create(m_rightPanel, wxID_ANY, wxString::FromUTF8("\tТекст транскрипції"), wxDefaultPosition, wxSize(-1, 30));
    m_audioTextHighligh->Create(m_rightPanel, SELECTCOLORMODE, wxDefaultPosition, wxSize(100, 30), m_audioTextChoices);
    m_audioTextHighligh->SetSelection(0);
    m_textHSizer->Add(m_audioTextHeader, 1, wxLEFT, 10);
    m_textHSizer->Add(m_audioTextHighligh, 1, wxRIGHT, 10);
    m_rightSizer->Add(m_textHSizer, 0, wxEXPAND | wxBOTTOM, 10);
    // Might need to change this text field to something else for colors
    // TODO: if there is time, I would love to fix user updates, but idk
    m_audioText->Create(m_rightPanel, TRANSCRIPTIONMOD, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_WORDWRAP | wxTE_RICH2 | wxTE_READONLY);
    m_rightSizer->Add(m_audioText, 2, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 10);

    // Bottom: Encryption Settings
    // Format
    m_outputFormatHeader->Create(m_rightPanel, wxID_ANY, wxString::FromUTF8("\tКінцевий формат:"), wxDefaultPosition, wxSize(-1, 30));
    m_outputFormatChoice->Create(m_rightPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 30), m_formats);
    m_outputFormatChoice->SetSelection(0);
    // Decrypt Toggle
    m_decryptChoice->Create(m_rightPanel, wxID_ANY, wxString::FromUTF8("Дешифрування?"), wxDefaultPosition, wxSize(30, -1));
    // Password
    m_passwordHeader->Create(m_rightPanel, wxID_ANY, wxString::FromUTF8("\t\tПароль:"), wxDefaultPosition, wxSize(-1, 30));
    m_passwordText->Create(m_rightPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(150, 30), wxTE_PASSWORD);
    m_modSizer->Add(m_outputFormatHeader, 1, wxLEFT, 5);
    m_modSizer->Add(m_outputFormatChoice, 1, wxLEFT, 5);
    m_modSizer->Add(m_decryptChoice, 1, wxALL, 5);
    m_modSizer->Add(m_passwordHeader, 1, wxLEFT, 5);
    m_modSizer->Add(m_passwordText, 1, wxRIGHT, 5);
    m_rightSizer->Add(m_modSizer, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    m_rightPanel->SetSizer(m_rightSizer);
}

void MainFrame::AddFile(const wxString &path)
{
    // Store actual path invisibly as ClientData, display formatted string
    AudioUtils::AudioInfo info = AudioUtils::GetAudioInfo(path.ToStdString());
    if (info.channels == 0)
        return;
    wxString displayString = wxString::FromUTF8(wxString::Format("%s | %d-біт, %d КГц, %d Каналів",
                                                                 info.filename, info.bitDepth, info.sampleRate, info.channels));

    // Check if real path is already in the list
    bool exists = false;
    for (unsigned int i = 0; i < m_fileList->GetCount(); ++i)
    {
        wxStringClientData *data = static_cast<wxStringClientData *>(m_fileList->GetClientObject(i));
        if (data && data->GetData() == path)
            exists = true;
    }

    if (!exists)
    {
        m_fileList->Append(displayString, new wxStringClientData(path));
        UpdateState();
    }
}

void MainFrame::OnFileSelected(wxCommandEvent &event)
{
    int selection = m_fileList->GetSelection();
    if (selection != wxNOT_FOUND)
    {
        UpdateState();
        wxStringClientData *data = static_cast<wxStringClientData *>(m_fileList->GetClientObject(selection));
        if (data)
        {
            wxString filepath = data->GetData();

            // Load waveform visuals into the multi-track control
            m_multiTrackCtrl->LoadTracks(filepath);

            // Extract full raw PCM samples and initialize the AudioPlayer (miniaudio)
            AudioUtils::AudioInfo info = AudioUtils::GetAudioInfo(filepath.ToStdString());
            std::vector<std::vector<float>> allChannelsData = AudioUtils::GetAllChannelSamples(filepath.ToStdString());

            if (m_audioPlayer.Initialize(info.sampleRate, info.channels))
                m_audioPlayer.LoadAudio(allChannelsData);
        }
    }
}

void MainFrame::OnOpen(wxCommandEvent &event)
{
    wxFileDialog openFileDialog(this, _("Select audio files"), "", "",
                                "Audio files (*.wav;*.mp3;*.opus)|*.wav;*.mp3;*.opus",
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxArrayString paths;
    openFileDialog.GetPaths(paths);
    for (size_t i = 0; i < paths.GetCount(); i++)
        AddFile(paths[i]);
}

void MainFrame::OnRemove(wxCommandEvent &event)
{
    int selection = m_fileList->GetSelection();
    if (selection != wxNOT_FOUND)
    {
        // Need to destroy player, reset multitrack, clear transcription and password
        m_audioPlayer.Pause();
        m_multiTrackCtrl->ClearZones();
        m_multiTrackCtrl->ClearTracks();
        m_multiTrackCtrl->SetSeekPos(0);
        m_audioText->Clear();
        m_passwordText->Clear();
        m_textBank->ClearLastTranscription();

        m_fileList->Delete(selection);
        UpdateState();
    }
}

void MainFrame::OnSettings(wxCommandEvent &event)
{
#ifdef _DEBUG
    if (m_textBank->GetKeywords().empty())
        m_textBank->InsertKeyword("test");
#endif
    SettingsDialog dlg(this);

    if (dlg.ShowModal() == wxID_OK)
    {
        if (dlg.GetModelSelection() == -1)
            wxMessageBox(wxString::FromUTF8("Failed to apply new settings"), wxString::FromUTF8("Info"));
        wxMessageBox(wxString::FromUTF8("Applying new settings"), wxString::FromUTF8("Info"));
        m_pathModel = dlg.GetModelPath();
        m_selectedModel.name = static_cast<TranscribeMethod::Models>(dlg.GetModelSelection());
        m_selectedModel.pathToModel = m_pathModel;
        m_useGPU = dlg.GetUseGPU();
        m_transcriber->ChangeModel(m_selectedModel);
        m_useGPU ? m_transcriber->EnableGPU() : m_transcriber->DisableGPU();
        auto words = m_textBank->SplitByWhitespace(dlg.GetKeywords());
        m_textBank->ClearKeywords();
        if (!words.empty())
            for (auto word : words)
                m_textBank->InsertKeyword(word);
    }
}

void MainFrame::OnTranscribe(wxCommandEvent &event)
{
    int selection = m_fileList->GetSelection();
    if (selection == wxNOT_FOUND)
    {
        wxMessageBox(wxString::FromUTF8("Будь ласка, виберіть аудіофайл зі списку."), wxString::FromUTF8("Попередження"), wxOK | wxICON_WARNING);
        return;
    }

    wxStringClientData *data = static_cast<wxStringClientData *>(m_fileList->GetClientObject(selection));
    if (!data)
        return;
    std::string filepath = data->GetData().ToStdString();

    if (m_trThread.joinable())
        m_trThread.join();

    m_btnTranscribe->Disable();
    m_btnOpen->Disable();
    m_btnRemove->Disable();

    m_trThread = std::thread([this, filepath]()
                             {
        m_textBank->ClearLastTranscription();
        m_transcriber->Transcribe(filepath, *m_textBank);
        std::string transcribedResult = m_textBank->GetFullText();

        wxTheApp->CallAfter([this, transcribedResult]() {
            m_btnTranscribe->Enable();
            m_btnOpen->Enable();
            m_btnRemove->Enable();

            if (!m_textBank->GetFullText().empty())
            {
                wxMessageBox(
                    wxString::FromUTF8("Транскрипцію успішно завершено!"), 
                    wxString::FromUTF8("Інформація"), 
                    wxOK | wxICON_INFORMATION
                );
                m_isUpdatingText = true;
                m_audioText->SetValue(wxString::FromUTF8(transcribedResult.c_str()));
                ApplyTextColors();
                m_isUpdatingText = false;
            } else {
                wxMessageBox(wxString::FromUTF8("Транскрипція невдала!"),
                    wxString::FromUTF8("Помилка"),
                    wxOK | wxICON_ERROR
                );
            }
        });
    });
}

void MainFrame::OnModify(wxCommandEvent &event)
{
    std::string savePath("");
    // MP3
    if (m_outputFormatChoice->GetSelection() == 0)
    {
        wxFileDialog saveFileDialog(this,
                                    wxString::FromUTF8("Зберегти змінений аудіофайл як..."), // Dialog title
                                    "", "",                                                  // Default directory and default filename
                                    "MP3 files (*.mp3)|*.mp3|All files (*.*)|*.*",
                                    wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        // If user clicks Cancel, exit out
        if (saveFileDialog.ShowModal() == wxID_CANCEL)
            return;
        savePath = saveFileDialog.GetPath().ToStdString();
    }
    else if (m_outputFormatChoice->GetSelection() == 1)
    {
        // Opus
        wxFileDialog saveFileDialog(this,
                                    wxString::FromUTF8("Зберегти змінений аудіофайл як..."), // Dialog title
                                    "", "",                                                  // Default directory and default filename
                                    "Opus files (*.opus)|*.opus|All files (*.*)|*.*",
                                    wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        // If user clicks Cancel, exit out
        if (saveFileDialog.ShowModal() == wxID_CANCEL)
            return;

        savePath = saveFileDialog.GetPath().ToStdString();
    }
    else
    {
        // Unknown
        return;
    }

    int selection = m_fileList->GetSelection();
    if (selection == wxNOT_FOUND)
        return;
    wxStringClientData *data = static_cast<wxStringClientData *>(m_fileList->GetClientObject(selection));
    if (!data)
        return;
    const std::string inputFilePath = data->GetData().ToStdString();

    if (savePath.empty())
        return;

    if (m_enThread.joinable())
        m_enThread.join();

    m_btnEncrypt->Disable();
    m_btnOpen->Disable();
    m_btnRemove->Disable();

    m_enThread = std::thread([this, savePath, inputFilePath]()
                             {
        std::vector<uint8_t> outBuf;
        ModificationMethod::Format format = m_outputFormatChoice->GetSelection() ? 
                                            ModificationMethod::Format::AUDIO_OPUS :
                                            ModificationMethod::Format::AUDIO_MP3;

        std::vector<TrackData> tracks = m_multiTrackCtrl->GetTracks();
        int width = m_multiTrackCtrl->GetLogicalWidth();
        size_t totalSamples = m_multiTrackCtrl->GetTotalSamples();
        uint64_t timeStamp1, timeStamp2;
        uint64_t trackId = 0;
        uint64_t totalDurationUs = (static_cast<uint64_t>(totalSamples) * 1000000ULL) / m_multiTrackCtrl->GetSampleRate();
        std::vector<ModificationMethod::TrackZones> allZones {};
        for (auto track : tracks)
        {
            std::vector<ModificationMethod::Zones> zones;
            for (auto zone : track.zones)
            {
                timeStamp1 = static_cast<uint64_t>((static_cast<double>(zone.start) / width * totalDurationUs));
                timeStamp2 = static_cast<uint64_t>((static_cast<double>(zone.end) / width * totalDurationUs));
                zones.push_back(ModificationMethod::Zones {timeStamp1, timeStamp2});
            }
            allZones.push_back(ModificationMethod::TrackZones { trackId, zones });
            trackId++;
        }

        const std::string pass = m_passwordText->GetValue().ToStdString();
        bool result = false;
        if (m_decryptChoice->GetValue())
        {
            result = m_encryptor->DoDecrypt(inputFilePath, pass, format, outBuf);
        } else {
            result = m_encryptor->DoEncrypt(inputFilePath, pass, format, allZones, outBuf);
        }

        wxTheApp->CallAfter([this, savePath, result, outBuf]()
        {
            m_btnEncrypt->Enable();
            m_btnOpen->Enable();
            m_btnRemove->Enable();

            if (result)
            {
                std::ofstream outFile(savePath, std::ios::binary);
                if (!outBuf.empty())
                    outFile.write(reinterpret_cast<const char*>(outBuf.data()), static_cast<std::streamsize>(outBuf.size()));

                wxMessageBox(
                    wxString::Format(wxString::FromUTF8("Файл успішно збережено:\n%s"), savePath), 
                    wxString::FromUTF8("Модифікація / Збереження"), 
                    wxOK | wxICON_INFORMATION
                );
            } else {
                wxMessageBox(
                    wxString::FromUTF8("Процес модифікації повернув помилку!"), 
                    wxString::FromUTF8("Модифікація / Збереження"), 
                    wxOK | wxICON_INFORMATION
                );
            }
        });
    });
}

void MainFrame::UpdateState()
{
    bool fileSelected = m_fileList->GetSelection() >= 0;

    m_btnTranscribe->Enable(fileSelected);
    m_btnEncrypt->Enable(fileSelected);
    m_btnRemove->Enable(fileSelected);
    m_mainPanel->GetParent()->SetMinSize(fileSelected ? wxSize(m_expandedWidth, m_baseHeight) : wxSize(m_baseWidth, m_baseHeight));

    wxSize currentSize = GetSize();
    if (fileSelected && !m_isExpanded)
    {
        m_rightPanel->Show(true);
        SetSize(m_expandedWidth, currentSize.GetHeight());
        m_isExpanded = true;
    }
    else if (!fileSelected && m_isExpanded)
    {
        m_rightPanel->Show(false);
        SetSize(m_baseWidth, currentSize.GetHeight());
        m_isExpanded = false;
    }
    m_mainPanel->GetParent()->Layout();
    m_mainPanel->GetParent()->Refresh();
    m_mainPanel->GetParent()->Update();
}

void MainFrame::LoadDebugText()
{
    m_textBank->ClearLastTranscription();
    m_audioText->Clear();

    // Development Mock Data
    m_textBank->LoadWordFromTranscription("test", 0.0f, -1, -1);
    m_textBank->LoadWordFromTranscription(" str", 1.0f, -1, -1);
    m_textBank->LoadWordFromTranscription("ing ", 0.5f, -1, -1);
    m_textBank->LoadWordFromTranscription("123", -1.0f, -1, -1);
    m_textBank->InsertKeyword("test");

    // Update the UI without triggering the user-input tracker
    m_isUpdatingText = true;
    m_audioText->SetValue(wxString::FromUTF8(m_textBank->GetFullText().c_str()));
    ApplyTextColors();
    m_isUpdatingText = false;
}

void MainFrame::OnColorModeChanged(wxCommandEvent &event)
{
    ApplyTextColors();
}

void MainFrame::OnTranscriptionTextChanged(wxCommandEvent &event)
{
    // Ignore events fired by our own SetValue() or SetStyle()
    if (m_isUpdatingText)
        return;

    std::string currentText = std::string(m_audioText->GetValue().ToUTF8());
    m_textBank->UpdateFromUserInput(currentText);

    // Reapply colors to maintain formatting around the typed text
    ApplyTextColors();
}

void MainFrame::ApplyTextColors()
{
    m_isUpdatingText = true; // Prevent EVT_TEXT from firing
    m_audioText->Freeze();   // Stop UI flickering

    // Reset everything to black text
    long lastPos = m_audioText->GetLastPosition();
    m_audioText->SetStyle(0, lastPos, wxTextAttr(*wxBLACK));

    int mode = m_audioTextHighligh->GetSelection();
    if (mode == 0)
    {
        wxString text = m_audioText->GetValue();
        long currentPos = 0;
        long sizeOfColor = 0;
        const auto &words = m_textBank->GetWords();

        for (auto word : words)
        {
            float val = word.prob;
            sizeOfColor = word.word.size();
            TextUtils::RGB rgb = TextUtils::GetColorForProb(val);
            m_audioText->SetStyle(currentPos, currentPos + sizeOfColor, wxTextAttr(wxColor(rgb.r, rgb.g, rgb.b)));
            currentPos += sizeOfColor;
        }
    }
    if (mode == 1)
    {
        wxString text = m_audioText->GetValue();
        long currentPos = 0;
        long sizeOfColor = 0;
        const auto &words = m_textBank->SplitByWhitespace(text.ToStdString());

        for (auto word : words)
        {
            TextUtils::RGB rgb = m_textBank->GetColorKeyword(word);
            sizeOfColor = word.size();
            m_audioText->SetStyle(currentPos, currentPos + sizeOfColor, wxTextAttr(wxColor(rgb.r, rgb.g, rgb.b)));
            currentPos += sizeOfColor + 1;
        }
    }

    m_audioText->Thaw(); // Resume UI painting
    m_isUpdatingText = false;
}

void MainFrame::OnPlay(wxCommandEvent &event)
{
    m_audioPlayer.Play();
}

void MainFrame::OnPause(wxCommandEvent &event)
{
    m_audioPlayer.Pause();
}

void MainFrame::OnSeekBegin(wxCommandEvent &event)
{
    m_multiTrackCtrl->SetSeekPos(0);
    m_audioPlayer.SeekToRatio(0.0f);
}

void MainFrame::OnSeekEnd(wxCommandEvent &event)
{
    m_multiTrackCtrl->SetSeekPos(m_multiTrackCtrl->GetLogicalWidth());
    m_audioPlayer.SeekToRatio(1.0f);
}

void MainFrame::OnResetZones(wxCommandEvent &event)
{
    m_multiTrackCtrl->ClearZones();
}

void MainFrame::OnVolumeChanged(wxCommandEvent &event)
{
    float vol = static_cast<float>(m_sldVolume->GetValue()) / 100.0f;
    m_audioPlayer.SetVolume(vol);
}

wxBEGIN_EVENT_TABLE(MultiTrackWaveformCtrl, wxScrolledWindow)
EVT_PAINT(MultiTrackWaveformCtrl::OnPaint)
EVT_LEFT_DOWN(MultiTrackWaveformCtrl::OnLeftDown)
EVT_LEFT_UP(MultiTrackWaveformCtrl::OnLeftUp)
EVT_RIGHT_DOWN(MultiTrackWaveformCtrl::OnRightDown)
EVT_RIGHT_UP(MultiTrackWaveformCtrl::OnRightUp)
EVT_MOTION(MultiTrackWaveformCtrl::OnMouseMove)
EVT_SIZE(MultiTrackWaveformCtrl::OnSize)
EVT_SCROLLWIN(MultiTrackWaveformCtrl::OnScroll)
wxEND_EVENT_TABLE()

MultiTrackWaveformCtrl::MultiTrackWaveformCtrl(wxWindow *parent, MainFrame *owner, wxWindowID id)
    : wxScrolledWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxHSCROLL | wxBORDER_SUNKEN),
      m_owner(owner)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Strictly enforce double-buffering
    // Helps with fixing the scrolling
    SetDoubleBuffered(true);
    EnableScrolling(false, false);
    ClearTracks();
}

void MultiTrackWaveformCtrl::UpdateVirtualSize()
{
    int clientHeight = GetClientSize().GetHeight();
    if (clientHeight < 50)
        clientHeight = 100;

    m_trackHeight = clientHeight / 2;
    int totalHeight = std::max(2, (int)m_tracks.size()) * m_trackHeight;
    int logicalWidth = HEADER_WIDTH + GetLogicalWidth();

    SetScrollbars(10, 10, logicalWidth / 10, totalHeight / 10);
}

void MultiTrackWaveformCtrl::OnScroll(wxScrollWinEvent &event)
{
    Refresh();    // Force a full, clean repaint
    event.Skip(); // Let wxWidgets continue updating the scrollbar mechanics
}

void MultiTrackWaveformCtrl::OnSize(wxSizeEvent &event)
{
    UpdateVirtualSize();
    Refresh();
    event.Skip();
}

void MultiTrackWaveformCtrl::ClearTracks()
{
    m_tracks.clear();
    m_totalSamples = 0;
    TrackData emptyTrack;
    m_tracks.push_back(emptyTrack);
    UpdateVirtualSize();
    Refresh();
}

void MultiTrackWaveformCtrl::LoadTracks(const wxString &filepath)
{
    m_tracks.clear();
    m_seekPos = 0;

    AudioUtils::AudioInfo info = AudioUtils::GetAudioInfo(filepath.ToStdString());
    m_sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100;

    auto allChannelsSamples = AudioUtils::GetAllChannelSamples(filepath.ToStdString());
    size_t maxSamples = 0;
    for (const auto &ch : allChannelsSamples)
        if (ch.size() > maxSamples)
            maxSamples = ch.size();
    m_totalSamples = maxSamples;

    for (int ch = 1; ch <= info.channels; ++ch)
    {
        TrackData track;
        track.wave = AudioUtils::GetWaveformData(filepath.ToStdString(), ch);
        m_tracks.push_back(track);
    }

    UpdateVirtualSize();
    Refresh();
}

void MultiTrackWaveformCtrl::OnPaint(wxPaintEvent &event)
{
    wxAutoBufferedPaintDC dc(this);
    DoPrepareDC(dc);
    dc.Clear();

    int scrollX, scrollY;
    GetViewStart(&scrollX, &scrollY);
    scrollX *= 10;

    int waveWidth = GetLogicalWidth();

    for (size_t i = 0; i < m_tracks.size(); ++i)
    {
        int yOffset = i * m_trackHeight;
        int midY = yOffset + (m_trackHeight / 2);
        const auto &track = m_tracks[i];

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColor(255, 200, 200, 128)));
        for (const auto &zone : track.zones)
        {
            int zStart = std::min(zone.start, zone.end);
            int zWidth = std::abs(zone.end - zone.start);
            dc.DrawRectangle(HEADER_WIDTH + zStart, yOffset, zWidth, m_trackHeight);
        }

        if (m_isSelecting && m_dragTrackIdx == (int)i)
        {
            int zStart = std::min(m_dragStart, m_dragEnd);
            int zWidth = std::abs(m_dragEnd - m_dragStart);
            dc.SetBrush(wxBrush(wxColor(255, 150, 150, 180)));
            dc.DrawRectangle(HEADER_WIDTH + zStart, yOffset, zWidth, m_trackHeight);
        }

        dc.SetPen(wxPen(track.isMuted ? wxColor(150, 150, 150) : wxColor(50, 150, 200), 1));
        if (track.wave.empty())
        {
            dc.DrawLine(HEADER_WIDTH, midY, HEADER_WIDTH + waveWidth, midY);
        }
        else
        {
            int points = track.wave.size();
            for (int x = 0; x < waveWidth; ++x)
            {
                int idx = (x * points) / waveWidth;
                if (idx >= points)
                    idx = points - 1;

                float peak = track.wave[idx];
                int halfHeight = static_cast<int>(peak * (m_trackHeight / 2.0f) * 0.85f);
                int yTop = midY - halfHeight;
                int yBottom = midY + halfHeight;
                if (yTop == yBottom)
                    yBottom = yTop + 1;

                dc.DrawLine(HEADER_WIDTH + x, yTop, HEADER_WIDTH + x, yBottom);
            }
        }

        dc.SetPen(wxPen(wxColor(200, 200, 200), 1));
        dc.DrawLine(0, yOffset + m_trackHeight, HEADER_WIDTH + waveWidth, yOffset + m_trackHeight);
    }

    dc.SetPen(wxPen(*wxBLACK, 2));
    int seekPixelX = HEADER_WIDTH + m_seekPos;
    dc.DrawLine(seekPixelX, 0, seekPixelX, std::max(2, (int)m_tracks.size()) * m_trackHeight);

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    dc.DrawRectangle(scrollX, 0, HEADER_WIDTH, std::max(2, (int)m_tracks.size()) * m_trackHeight);

    dc.SetPen(wxPen(wxColor(150, 150, 150), 1));
    dc.DrawLine(scrollX + HEADER_WIDTH - 1, 0, scrollX + HEADER_WIDTH - 1, std::max(2, (int)m_tracks.size()) * m_trackHeight);

    for (size_t i = 0; i < m_tracks.size(); ++i)
    {
        int yOffset = i * m_trackHeight;
        dc.SetTextForeground(*wxBLACK);
        dc.DrawText(wxString::Format("%zu", i + 1), scrollX + 15, yOffset + (m_trackHeight / 2) - 10);

        dc.SetBrush(m_tracks[i].isMuted ? *wxRED_BRUSH : *wxWHITE_BRUSH);
        dc.DrawRectangle(scrollX + 45, yOffset + (m_trackHeight / 2) - 10, 20, 20);

        dc.SetTextForeground(m_tracks[i].isMuted ? *wxWHITE : *wxBLACK);
        dc.DrawText("M", scrollX + 49, yOffset + (m_trackHeight / 2) - 8);
    }
}

void MultiTrackWaveformCtrl::OnLeftDown(wxMouseEvent &event)
{
    int logicalX, logicalY;
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);

    int scrollX = GetViewStart().x * 10;
    int maxW = GetLogicalWidth();

    if (logicalX < scrollX + HEADER_WIDTH)
    {
        int trackIdx = logicalY / m_trackHeight;
        if (trackIdx >= 0 && trackIdx < (int)m_tracks.size())
            if (logicalX > scrollX + 40 && logicalX < scrollX + 70)
                m_tracks[trackIdx].isMuted = !m_tracks[trackIdx].isMuted;
    }
    else
    {
        int waveX = logicalX - HEADER_WIDTH;
        m_seekPos = std::max(0, std::min(maxW, waveX));
        m_isSelecting = true;
        CaptureMouse();
    }
    Refresh();
}

void MultiTrackWaveformCtrl::OnLeftUp(wxMouseEvent &event)
{
    if (m_isSelecting)
    {
        int logicalX, logicalY;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);

        int scrollX = GetViewStart().x * 10;
        int maxW = GetLogicalWidth();
        if (logicalX >= scrollX + HEADER_WIDTH)
        {
            int waveX = logicalX - HEADER_WIDTH;
            m_seekPos = std::max(0, std::min(maxW, waveX));
            if (m_totalSamples > 0 && maxW > 0)
            {
                size_t targetFrame = static_cast<size_t>((static_cast<double>(m_seekPos) / maxW) * m_totalSamples);
                if (m_owner)
                    m_owner->SeekAudioFromWaveform(targetFrame);
            }
        }

        m_isSelecting = false;
        if (HasCapture())
            ReleaseMouse();
        Refresh();
    }
}

void MultiTrackWaveformCtrl::OnRightDown(wxMouseEvent &event)
{
    int logicalX, logicalY;
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);

    int scrollX = GetViewStart().x * 10;
    int maxW = GetLogicalWidth();
    if (logicalX >= scrollX + HEADER_WIDTH)
    {
        m_dragTrackIdx = logicalY / m_trackHeight;
        if (m_dragTrackIdx >= 0 && m_dragTrackIdx < (int)m_tracks.size())
        {
            m_dragStart = std::max(0, std::min(maxW, logicalX - HEADER_WIDTH));
            m_dragEnd = m_dragStart;
            m_isSelecting = true;
            CaptureMouse();
            Refresh();
        }
    }
}

void MultiTrackWaveformCtrl::OnMouseMove(wxMouseEvent &event)
{
    if (m_isSelecting && event.Dragging() && event.RightIsDown())
    {
        int logicalX, logicalY;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);
        int maxW = GetLogicalWidth();
        m_dragEnd = std::max(0, std::min(maxW, logicalX - HEADER_WIDTH));
        Refresh();
    }
    if (m_isSelecting && event.Dragging() && event.LeftIsDown())
    {
        int logicalX, logicalY;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);
        int maxW = GetLogicalWidth();
        int waveX = logicalX - HEADER_WIDTH;
        m_seekPos = std::max(0, std::min(maxW, waveX));
        Refresh();
    }
}

void MultiTrackWaveformCtrl::OnRightUp(wxMouseEvent &event)
{
    if (m_isSelecting)
    {
        int logicalX, logicalY;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);
        int maxW = GetLogicalWidth();
        m_dragEnd = std::max(0, std::min(maxW, logicalX - HEADER_WIDTH));
        m_isSelecting = false;

        if (HasCapture())
            ReleaseMouse();

        if (std::abs(m_dragEnd - m_dragStart) > 2)
        {
            TrackZone newZone = {std::min(m_dragStart, m_dragEnd), std::max(m_dragStart, m_dragEnd)};
            m_tracks[m_dragTrackIdx].zones.push_back(newZone);
            MergeZones(m_dragTrackIdx);
        }
        Refresh();
    }
}

void MultiTrackWaveformCtrl::MergeZones(int trackIdx)
{
    auto &zones = m_tracks[trackIdx].zones;
    if (zones.empty())
        return;

    // Sort by start time
    std::sort(zones.begin(), zones.end(), [](const TrackZone &a, const TrackZone &b)
              { return a.start < b.start; });

    std::vector<TrackZone> merged;
    merged.push_back(zones[0]);

    // Combine overlapping intervals
    for (size_t i = 1; i < zones.size(); ++i)
    {
        auto &last = merged.back();
        if (zones[i].start <= last.end)
            last.end = std::max(last.end, zones[i].end);
        else
            merged.push_back(zones[i]);
    }
    zones = merged;
}

void MultiTrackWaveformCtrl::ClearZones()
{
    for (auto &track : m_tracks)
        track.zones.clear();
    Refresh();
}

int MultiTrackWaveformCtrl::GetLogicalWidth() const
{
    if (m_totalSamples == 0 || m_sampleRate == 0)
        return 900; // Default baseline (30s at 30px/s)
    double durationSeconds = static_cast<double>(m_totalSamples) / m_sampleRate;
    const int PIXELS_PER_SECOND = 30; // 30 seconds = 900 pixels width baseline
    return std::max(100, static_cast<int>(durationSeconds * PIXELS_PER_SECOND));
}

void MultiTrackWaveformCtrl::SetSeekPos(int pos)
{
    m_seekPos = std::max(0, std::min(GetLogicalWidth(), pos));
    Refresh();
}

void MainFrame::SeekAudioFromWaveform(size_t frameIndex)
{
    m_audioPlayer.SeekToFrame(frameIndex);
}

SettingsDialog::SettingsDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxString::FromUTF8("Налаштування"), wxDefaultPosition, wxSize(settings_window_width, settings_window_height))
{
    BuildUI();
    CenterOnParent();
}

void SettingsDialog::BuildUI()
{
    m_grid->AddGrowableCol(1, 1); // Make the 2nd column (controls) expand to fill space

    m_modelPathLabel->Create(this, wxID_ANY, wxString::FromUTF8("Шлях до моделі:"));
    m_modelPath->Create(this, wxID_ANY, wxEmptyString);
    m_grid->Add(m_modelPathLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_grid->Add(m_modelPath, 1, wxEXPAND);
    m_modelPath->SetValue(((MainFrame *)this->GetParent())->GetModelPath());
    m_modelOptionLabel->Create(this, wxID_ANY, wxString::FromUTF8("Модель:"));
    m_modelSelection->Create(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_modelOptions);
    m_modelSelection->SetSelection(((MainFrame *)this->GetParent())->GetSelectedModel().name);
    m_grid->Add(m_modelOptionLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_grid->Add(m_modelSelection, 1, wxEXPAND);
    m_useGPU->Create(this, wxID_ANY, wxString::FromUTF8("Використовувати GPU"));
    m_useGPU->SetValue(((MainFrame *)this->GetParent())->GetUseGPU());
    m_grid->Add(new wxStaticText(this, wxID_ANY, ""), 0, wxALIGN_CENTER_VERTICAL);
    m_grid->Add(m_useGPU, 1, wxEXPAND);
    m_keywordsLabel->Create(this, wxID_ANY, wxString::FromUTF8("Ключові слова (через пробіл):"));
    m_keywords->Create(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 150), wxTE_MULTILINE | wxTE_WORDWRAP);
    m_keywords->SetValue(((MainFrame *)this->GetParent())->GetKeywords());
    m_grid->Add(m_keywordsLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_grid->Add(m_keywords, 1, wxEXPAND);

    m_mainSizer->Add(m_grid, 1, wxEXPAND | wxALL, 20);

    if (m_btnSizer)
        m_mainSizer->Add(m_btnSizer, 0, wxEXPAND | wxALL, 10);

    SetSizer(m_mainSizer);
}

std::string SettingsDialog::GetModelPath()
{
    return m_modelPath->GetValue().ToStdString();
}

int SettingsDialog::GetModelSelection()
{
    int choice = m_modelSelection->GetSelection();
    switch (choice)
    {
    case TranscribeMethod::Models::GGML_WHISPER_TINY:
        return TranscribeMethod::Models::GGML_WHISPER_TINY;
    case TranscribeMethod::Models::GGML_WHISPER_BASE:
        return TranscribeMethod::Models::GGML_WHISPER_BASE;
    case TranscribeMethod::Models::GGML_WHISPER_SMALL:
        return TranscribeMethod::Models::GGML_WHISPER_SMALL;
    case TranscribeMethod::Models::GGML_WHISPER_MEDIUM:
        return TranscribeMethod::Models::GGML_WHISPER_MEDIUM;
    case TranscribeMethod::Models::GGML_WHISPER_LARGEv2:
        return TranscribeMethod::Models::GGML_WHISPER_LARGEv2;
    case TranscribeMethod::Models::GGML_WHISPER_LARGEv3:
        return TranscribeMethod::Models::GGML_WHISPER_LARGEv3;
    case TranscribeMethod::Models::GGML_WHISPER_LARGEv3_TURBO:
        return TranscribeMethod::Models::GGML_WHISPER_LARGEv3_TURBO;
    default:
        return -1;
    };
}

bool SettingsDialog::GetUseGPU()
{
    return m_useGPU;
}

std::string SettingsDialog::GetKeywords()
{
    return m_keywords->GetValue().ToStdString();
}
