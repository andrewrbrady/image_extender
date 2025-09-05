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
    // Section: Mode selector
    auto* modeBox = new wxStaticBoxSizer(wxVERTICAL, this, "Mode");
    auto* modeRow = new wxBoxSizer(wxHORIZONTAL);
    modeRow->Add(new wxStaticText(this, wxID_ANY, "Feature:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    modeBox_ = new wxComboBox(this, wxID_ANY);
    modeBox_->Append("Extend Canvas");
    modeBox_->Append("Vehicle Mask (SAM2)");
    modeBox_->Append("Crop");
    modeBox_->SetSelection(0);
    modeRow->Add(modeBox_, 1);
    modeBox->Add(modeRow, 0, wxALL | wxEXPAND, 6);
    root->Add(modeBox, 0, wxEXPAND | wxALL, 6);
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
    // Common 1080-based presets
    addPreset("1080×1920 (9:16)", 1080, 1920);
    addPreset("1080×1350 (4:5)", 1080, 1350);
    addPreset("1080×1440 (3:4)", 1080, 1440);
    addPreset("1080×1620 (2:3)", 1080, 1620);
    addPreset("1080×1080 (1:1)", 1080, 1080);
    addPreset("1080×810 (4:3)", 1080, 810);
    addPreset("1080×720 (3:2)", 1080, 720);
    addPreset("1080×608 (16:9)", 1080, 608);
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
    whiteThrLabel_ = new wxStaticText(this, wxID_ANY, "White Threshold:");
    paramsRow->Add(whiteThrLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    whiteThr_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, -1, 255, 20);
    paramsRow->Add(whiteThr_, 0, wxRIGHT, 12);
    paddingLabel_ = new wxStaticText(this, wxID_ANY, "Padding %:");
    paramsRow->Add(paddingLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
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

    // Crop settings: aspect is implicitly derived from canvas Width/Height

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

    // Section: Masking parameters (visible for Vehicle Mask mode)
    maskBox_ = new wxStaticBoxSizer(wxVERTICAL, this, "Masking (Vehicle Mask)");
    auto* maskRow1 = new wxBoxSizer(wxHORIZONTAL);
    maskRow1->Add(new wxStaticText(this, wxID_ANY, "Canny Low:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    cannyLow_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), wxSP_ARROW_KEYS, 0, 1000, 50);
    maskRow1->Add(cannyLow_, 0, wxRIGHT, 10);
    maskRow1->Add(new wxStaticText(this, wxID_ANY, "Canny High:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    cannyHigh_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), wxSP_ARROW_KEYS, 0, 2000, 150);
    maskRow1->Add(cannyHigh_, 0, wxRIGHT, 10);
    maskRow1->Add(new wxStaticText(this, wxID_ANY, "Kernel:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    morphKernel_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 1, 99, 7);
    maskRow1->Add(morphKernel_, 0, wxRIGHT, 10);
    maskBox_->Add(maskRow1, 0, wxALL, 6);

    auto* maskRow2 = new wxBoxSizer(wxHORIZONTAL);
    maskRow2->Add(new wxStaticText(this, wxID_ANY, "Dilate:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    dilateIters_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 0, 50, 2);
    maskRow2->Add(dilateIters_, 0, wxRIGHT, 10);
    maskRow2->Add(new wxStaticText(this, wxID_ANY, "Erode:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    erodeIters_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 0, 50, 0);
    maskRow2->Add(erodeIters_, 0, wxRIGHT, 10);
    maskRow2->Add(new wxStaticText(this, wxID_ANY, "Min area:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    minArea_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90,-1), wxSP_ARROW_KEYS, 0, 20000000, 5000);
    maskRow2->Add(minArea_, 0, wxRIGHT, 10);
    maskBox_->Add(maskRow2, 0, wxALL, 6);

    auto* maskRow3 = new wxBoxSizer(wxHORIZONTAL);
    whiteCycAssist_ = new wxCheckBox(this, wxID_ANY, "White cyc assist");
    whiteCycAssist_->SetValue(true);
    maskRow3->Add(whiteCycAssist_, 0, wxRIGHT, 10);
    maskRow3->Add(new wxStaticText(this, wxID_ANY, "White thr:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    whiteThrMask_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), wxSP_ARROW_KEYS, -1, 255, -1);
    maskRow3->Add(whiteThrMask_, 0, wxRIGHT, 10);
    maskRow3->Add(new wxStaticText(this, wxID_ANY, "Feather:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    featherRadiusMask_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), wxSP_ARROW_KEYS, 0, 50, 0);
    maskRow3->Add(featherRadiusMask_, 0, wxRIGHT, 10);
    invertMask_ = new wxCheckBox(this, wxID_ANY, "Invert output");
    invertMask_->SetValue(false);
    maskRow3->Add(invertMask_, 0, wxRIGHT, 10);
    maskBox_->Add(maskRow3, 0, wxALL, 6);

    root->Add(maskBox_, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

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
    // Also react to direct text edits in spin controls
    width_->Bind(wxEVT_TEXT, fireSettingsChanged);
    height_->Bind(wxEVT_TEXT, fireSettingsChanged);
    whiteThr_->Bind(wxEVT_TEXT, fireSettingsChanged);
    blurRadius_->Bind(wxEVT_TEXT, fireSettingsChanged);
    // Masking controls post to preview updates
    auto fireMaskChanged = [this](wxCommandEvent&){ wxCommandEvent ev(wxEVT_WXUI_SETTINGS_CHANGED); wxPostEvent(this, ev); };
    cannyLow_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    cannyHigh_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    morphKernel_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    dilateIters_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    erodeIters_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    // text edit bindings for responsive preview
    cannyLow_->Bind(wxEVT_TEXT, fireMaskChanged);
    cannyHigh_->Bind(wxEVT_TEXT, fireMaskChanged);
    morphKernel_->Bind(wxEVT_TEXT, fireMaskChanged);
    dilateIters_->Bind(wxEVT_TEXT, fireMaskChanged);
    erodeIters_->Bind(wxEVT_TEXT, fireMaskChanged);
    whiteCycAssist_->Bind(wxEVT_CHECKBOX, fireMaskChanged);
    whiteThrMask_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    whiteThrMask_->Bind(wxEVT_TEXT, fireMaskChanged);
    minArea_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    featherRadiusMask_->Bind(wxEVT_SPINCTRL, fireMaskChanged);
    minArea_->Bind(wxEVT_TEXT, fireMaskChanged);
    featherRadiusMask_->Bind(wxEVT_TEXT, fireMaskChanged);
    invertMask_->Bind(wxEVT_CHECKBOX, fireMaskChanged);
    modeBox_->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&){
        EnsureDefaultOutputFolder();
        const bool showMask = (getMode() == ProcessingMode::VehicleMask);
        if (maskBox_) maskBox_->ShowItems(showMask);
        const bool isCrop = (getMode() == ProcessingMode::Crop);
        // Hide white threshold and padding in Crop mode
        if (whiteThrLabel_) whiteThrLabel_->Show(!isCrop);
        if (whiteThr_) whiteThr_->Show(!isCrop);
        if (paddingLabel_) paddingLabel_->Show(!isCrop);
        if (padding_) padding_->Show(!isCrop);
        Layout();
        wxCommandEvent ev(wxEVT_WXUI_SETTINGS_CHANGED);
        wxPostEvent(this, ev);
    });

    // Initial visibility per mode (hide/show all items in the sizer)
    if (maskBox_) maskBox_->ShowItems(getMode() == ProcessingMode::VehicleMask);
    // Initial visibility: hide threshold/padding if starting in Crop (default is Extend)
    const bool startIsCrop = (getMode() == ProcessingMode::Crop);
    if (whiteThrLabel_) whiteThrLabel_->Show(!startIsCrop);
    if (whiteThr_) whiteThr_->Show(!startIsCrop);
    if (paddingLabel_) paddingLabel_->Show(!startIsCrop);
    if (padding_) padding_->Show(!startIsCrop);

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
    if (batchFiles_.IsEmpty()) return;
    wxFileName fn(batchFiles_[0]);
    const wxString defExtend = fn.GetPathWithSep() + "extended_images";
    const wxString defMasks  = fn.GetPathWithSep() + "masks";
    const bool wantMasks = (getMode() == ProcessingMode::VehicleMask);
    const wxString desired = wantMasks ? defMasks : defExtend;

    if (outputFolder_->IsEmpty()) {
        outputFolder_->ChangeValue(desired);
        return;
    }

    // If user hasn't customized (still at the other mode's default), switch to this mode's default
    if (!wantMasks && outputFolder_->GetValue() == defMasks) {
        outputFolder_->ChangeValue(desired);
    } else if (wantMasks && outputFolder_->GetValue() == defExtend) {
        outputFolder_->ChangeValue(desired);
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
    wxString p = outputFolder_->GetValue();
    p.Replace("\n", "");
    p.Replace("\r", "");
    p.Trim(true).Trim(false);
    return p;
}

wxArrayString WxControlPanel::getBatchFiles() const
{
    return batchFiles_;
}

ProcessingMode WxControlPanel::getMode() const
{
    int sel = modeBox_ ? modeBox_->GetSelection() : 0;
    if (sel == 1) return ProcessingMode::VehicleMask;
    if (sel == 2) return ProcessingMode::Crop;
    return ProcessingMode::ExtendCanvas;
}

MaskSettings WxControlPanel::getMaskSettings() const
{
    MaskSettings m;
    m.cannyLow = cannyLow_->GetValue();
    m.cannyHigh = cannyHigh_->GetValue();
    int k = morphKernel_->GetValue(); if (k % 2 == 0) k += 1; if (k < 1) k = 1; m.morphKernel = k;
    m.dilateIters = dilateIters_->GetValue();
    m.erodeIters = erodeIters_->GetValue();
    m.useWhiteCycAssist = whiteCycAssist_->GetValue();
    m.whiteThreshold = whiteThrMask_->GetValue();
    m.minArea = minArea_->GetValue();
    m.featherRadius = featherRadiusMask_->GetValue();
    m.invert = invertMask_->GetValue();
    return m;
}

double WxControlPanel::getCropAspectRatio() const
{
    int w = width_ ? width_->GetValue() : 0;
    int h = height_ ? height_->GetValue() : 0;
    if (w <= 0 || h <= 0) return 0.0;
    return double(w) / double(h);
}
