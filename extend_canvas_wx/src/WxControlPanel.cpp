#include "WxControlPanel.hpp"
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/filename.h>

// Define custom events
wxDEFINE_EVENT(wxEVT_WXUI_SETTINGS_CHANGED, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_WXUI_PROCESS_REQUESTED, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_WXUI_BATCH_ITEM_SELECTED, wxCommandEvent);

static bool IsImagePath(const wxString& p)
{
    wxFileName fn(p);
    wxString ext = fn.GetExt().Lower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tif" || ext == "tiff";
}

bool WxFileDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames)
{
    wxArrayString imgs;
    for (auto& f : filenames) if (IsImagePath(f)) imgs.Add(f);
    if (!imgs.IsEmpty())
    {
        owner_->AddFiles(imgs);
        return true;
    }
    return false;
}

WxControlPanel::WxControlPanel(wxWindow* parent) : wxPanel(parent)
{
    BuildUI();
    WireEvents();
}

void WxControlPanel::BuildUI()
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    // Section: Batch list + buttons
    auto* listLabel = new wxStaticText(this, wxID_ANY, "Batch Files");
    root->Add(listLabel, 0, wxALL, 6);

    list_ = new wxListBox(this, wxID_ANY);
    list_->SetMinSize(wxSize(280, 220));
    list_->SetDropTarget(new WxFileDropTarget(this));
    root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);

    auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
    addBtn_ = new wxButton(this, wxID_ANY, "Add Images...");
    clearBtn_ = new wxButton(this, wxID_ANY, "Clear");
    btnRow->Add(addBtn_, 0, wxRIGHT, 6);
    btnRow->Add(clearBtn_, 0);
    root->Add(btnRow, 0, wxALL, 6);

    progress_ = new wxGauge(this, wxID_ANY, 100);
    progress_->Hide();
    root->Add(progress_, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 6);

    // Section: Canvas dimensions (with presets)
    auto* dimsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Canvas Dimensions");
    auto* presetsRow = new wxBoxSizer(wxHORIZONTAL);
    auto addPreset = [&](const wxString& text, int w, int h){
        auto* b = new wxButton(this, wxID_ANY, text);
        b->Bind(wxEVT_BUTTON, [this, w, h](wxCommandEvent&){ width_->SetValue(w); height_->SetValue(h); wxCommandEvent ev(wxEVT_WXUI_SETTINGS_CHANGED); wxPostEvent(this, ev);} );
        presetsRow->Add(b, 0, wxRIGHT, 6);
    };
    addPreset("1080×1350", 1080, 1350);
    addPreset("1080×1920", 1080, 1920);
    addPreset("1080×1080", 1080, 1080);
    dimsBox->Add(presetsRow, 0, wxALL, 6);

    auto* dimsRow = new wxBoxSizer(wxHORIZONTAL);
    dimsRow->Add(new wxStaticText(this, wxID_ANY, "Width:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    width_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 20000, 1080);
    dimsRow->Add(width_, 0, wxRIGHT, 12);
    dimsRow->Add(new wxStaticText(this, wxID_ANY, "Height:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    height_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 20000, 1920);
    dimsRow->Add(height_, 0, wxRIGHT, 12);
    dimsBox->Add(dimsRow, 0, wxALL, 6);
    root->Add(dimsBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

    // Section: Processing params
    auto* paramsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Processing Parameters");
    auto* paramsRow = new wxBoxSizer(wxHORIZONTAL);
    paramsRow->Add(new wxStaticText(this, wxID_ANY, "White Threshold:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    whiteThr_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, -1, 255, 20);
    paramsRow->Add(whiteThr_, 0, wxRIGHT, 12);
    paramsRow->Add(new wxStaticText(this, wxID_ANY, "Padding %:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    padding_ = new wxSpinCtrlDouble(this, wxID_ANY);
    padding_->SetRange(0.0, 1.0);
    padding_->SetIncrement(0.01);
    padding_->SetDigits(3);
    padding_->SetValue(0.05);
    padding_->SetMinSize(wxSize(90, -1));
    paramsRow->Add(padding_, 0, wxRIGHT, 12);
    paramsRow->Add(new wxStaticText(this, wxID_ANY, "Blur radius:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    blurRadius_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 50, 0);
    paramsRow->Add(blurRadius_, 0, wxRIGHT, 12);
    paramsBox->Add(paramsRow, 0, wxALL, 6);

    auto* scaleRow = new wxBoxSizer(wxHORIZONTAL);
    scaleBox_ = new wxComboBox(this, wxID_ANY);
    scaleBox_->Append("1×");
    scaleBox_->Append("2×");
    scaleBox_->Append("4×");
    scaleBox_->SetSelection(0);
    processBtn_ = new wxButton(this, wxID_ANY, "Process Images");
    processBtn_->Enable(false);
    scaleRow->Add(scaleBox_, 0, wxRIGHT, 6);
    scaleRow->Add(processBtn_, 1);
    paramsBox->Add(scaleRow, 0, wxEXPAND | wxALL, 6);

    root->Add(paramsBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

    // Section: Output folder
    auto* outBox = new wxStaticBoxSizer(wxVERTICAL, this, "Output Folder");
    outputFolder_ = new wxTextCtrl(this, wxID_ANY);
    browseBtn_ = new wxButton(this, wxID_ANY, "Browse...");
    outBox->Add(outputFolder_, 0, wxEXPAND | wxBOTTOM, 6);
    outBox->Add(browseBtn_, 0, wxALIGN_LEFT);
    root->Add(outBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(root);
}

void WxControlPanel::WireEvents()
{
    addBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){
        wxFileDialog dlg(this, "Select Images for Processing", wxEmptyString, wxEmptyString,
                         "Image files (*.png;*.jpg;*.jpeg;*.bmp;*.tiff;*.tif)|*.png;*.jpg;*.jpeg;*.bmp;*.tiff;*.tif|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (dlg.ShowModal() == wxID_OK)
        {
            wxArrayString paths; dlg.GetPaths(paths);
            AddFiles(paths);
        }
    });
    clearBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){
        batchFiles_.Clear();
        list_->Clear();
        UpdateProcessEnabled();
    });
    list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent& ev){
        int i = ev.GetSelection();
        if (i >= 0 && i < (int)batchFiles_.size())
        {
            wxCommandEvent out(wxEVT_WXUI_BATCH_ITEM_SELECTED);
            out.SetString(batchFiles_[i]);
            wxPostEvent(this, out);
        }
    });

    auto fireSettingsChanged = [this](wxCommandEvent&){ wxCommandEvent ev(wxEVT_WXUI_SETTINGS_CHANGED); wxPostEvent(this, ev); };
    width_->Bind(wxEVT_SPINCTRL, fireSettingsChanged);
    height_->Bind(wxEVT_SPINCTRL, fireSettingsChanged);
    whiteThr_->Bind(wxEVT_SPINCTRL, fireSettingsChanged);
    padding_->Bind(wxEVT_SPINCTRLDOUBLE, fireSettingsChanged);
    blurRadius_->Bind(wxEVT_SPINCTRL, fireSettingsChanged);

    processBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ wxCommandEvent ev(wxEVT_WXUI_PROCESS_REQUESTED); wxPostEvent(this, ev); });

    browseBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){
        wxDirDialog dlg(this, "Select Output Folder", outputFolder_->GetValue());
        if (dlg.ShowModal() == wxID_OK) outputFolder_->ChangeValue(dlg.GetPath());
    });
}

void WxControlPanel::AddFiles(const wxArrayString& paths)
{
    size_t added = 0;
    for (auto& p : paths)
    {
        if (!IsImagePath(p)) continue;
        if (batchFiles_.Index(p) != wxNOT_FOUND) continue;
        batchFiles_.Add(p);
        list_->Append(wxFileName(p).GetFullName());
        ++added;
        // initialize default settings
        if (!perImageSettings_.count(p)) perImageSettings_[p] = ImageSettings();
    }
    if (added > 0)
    {
        EnsureDefaultOutputFolder();
        UpdateProcessEnabled();
        // auto select last added
        list_->SetSelection((int)list_->GetCount()-1);
        wxCommandEvent ev(wxEVT_WXUI_BATCH_ITEM_SELECTED); ev.SetString(batchFiles_.Last()); wxPostEvent(this, ev);
    }
}

void WxControlPanel::UpdateProcessEnabled()
{
    processBtn_->Enable(!batchFiles_.IsEmpty());
}

void WxControlPanel::EnsureDefaultOutputFolder()
{
    if (outputFolder_->IsEmpty() && !batchFiles_.IsEmpty())
    {
        wxFileName fn(batchFiles_[0]);
        wxString def = fn.GetPathWithSep() + "extended_images";
        outputFolder_->ChangeValue(def);
    }
}

ImageSettings WxControlPanel::getCurrentSettings() const
{
    ImageSettings s;
    s.width = width_->GetValue();
    s.height = height_->GetValue();
    s.whiteThreshold = whiteThr_->GetValue();
    s.padding = padding_->GetValue();
    s.blurRadius = blurRadius_->GetValue();
    s.finalWidth = -1; // optional; can be added to UI if needed
    s.finalHeight = -1;
    return s;
}

void WxControlPanel::loadSettings(const ImageSettings& settings)
{
    width_->SetValue(settings.width);
    height_->SetValue(settings.height);
    whiteThr_->SetValue(settings.whiteThreshold);
    padding_->SetValue(settings.padding);
    blurRadius_->SetValue(settings.blurRadius);
}

int WxControlPanel::getScaleFactor() const
{
    switch (scaleBox_->GetSelection())
    {
        case 1: return 2;
        case 2: return 4;
        default: return 1;
    }
}

wxString WxControlPanel::getOutputFolder() const
{
    return outputFolder_->GetValue();
}

wxArrayString WxControlPanel::getBatchFiles() const
{
    return batchFiles_;
}
