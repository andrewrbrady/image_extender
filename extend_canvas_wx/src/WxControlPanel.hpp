#pragma once
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/filepicker.h>
#include <wx/slider.h>
#include <vector>
#include <map>
#include <wx/dnd.h>

#include "models/ImageSettings.hpp"
#include "models/ProcessingMode.hpp"
#include "models/MaskSettings.hpp"

// Custom event declarations
wxDECLARE_EVENT(wxEVT_WXUI_SETTINGS_CHANGED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WXUI_PROCESS_REQUESTED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WXUI_BATCH_ITEM_SELECTED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WXUI_DEVELOP_REQUESTED, wxCommandEvent);

class WxFileDropTarget;

class WxControlPanel : public wxPanel
{
public:
    explicit WxControlPanel(wxWindow* parent);

    // Accessors mirroring Qt panel
    ImageSettings getCurrentSettings() const;
    void loadSettings(const ImageSettings& settings);
    int getScaleFactor() const; // 1, 2, 4
    wxString getOutputFolder() const;
    wxArrayString getBatchFiles() const;
    wxArrayString getTextureFiles() const;
    wxString getSelectedTexturePath() const;
    int getDevelopBlendMode() const; // 0=multiply,1=screen,2=lighten
    float getDevelopOpacity() const; // 0..1
    bool getRandomizeOnDevelop() const;
    void RandomizeDevelopParams();
    bool getUseTextureLuminance() const; // if true, convert texture to grayscale
    bool getSwapRB() const; // debug: swap R/B channels in texture
    ProcessingMode getMode() const;
    MaskSettings getMaskSettings() const;
    int getSplitterCount() const;

private:
    void BuildUI();
    void WireEvents();
    void AddFiles(const wxArrayString& paths);
    void AddTextures(const wxArrayString& paths);
    void UpdateProcessEnabled();
    void EnsureDefaultOutputFolder();

    // Controls
    wxListBox* list_ {nullptr};
    wxButton* addBtn_ {nullptr};
    wxButton* clearBtn_ {nullptr};
    wxGauge* progress_ {nullptr};

    // Film develop UI
    wxListBox* texList_ {nullptr};
    wxButton* texAddBtn_ {nullptr};
    wxButton* texClearBtn_ {nullptr};
    wxButton* developBtn_ {nullptr};
    wxComboBox* blendBox_ {nullptr};
    wxSlider* opacitySlider_ {nullptr};
    wxStaticText* opacityLabel_ {nullptr};
    wxButton* randomizeBtn_ {nullptr};
    wxCheckBox* randomOnDevelop_ {nullptr};
    wxCheckBox* useTexLuma_ {nullptr};
    wxCheckBox* swapRB_ {nullptr};

    wxSpinCtrl* width_ {nullptr};
    wxSpinCtrl* height_ {nullptr};
    wxSpinCtrl* whiteThr_ {nullptr};
    wxSpinCtrlDouble* padding_ {nullptr};
    wxSpinCtrl* blurRadius_ {nullptr};
    wxStaticText* whiteThrLabel_ {nullptr};
    wxStaticText* paddingLabel_ {nullptr};
    wxComboBox* scaleBox_ {nullptr};
    wxButton* processBtn_ {nullptr};

    wxComboBox* modeBox_ {nullptr};
    wxStaticText* splitsLabel_ {nullptr};
    wxSpinCtrl* splits_ {nullptr};
    wxCheckBox* stretchIfNeeded_ {nullptr};

    // Masking controls (shown in Vehicle Mask mode)
    wxStaticBoxSizer* maskBox_ {nullptr};
    wxSpinCtrl* cannyLow_ {nullptr};
    wxSpinCtrl* cannyHigh_ {nullptr};
    wxSpinCtrl* morphKernel_ {nullptr};
    wxSpinCtrl* dilateIters_ {nullptr};
    wxSpinCtrl* erodeIters_ {nullptr};
    wxCheckBox* whiteCycAssist_ {nullptr};
    wxSpinCtrl* whiteThrMask_ {nullptr};
    wxSpinCtrl* minArea_ {nullptr};
    wxSpinCtrl* featherRadiusMask_ {nullptr};
    wxCheckBox* invertMask_ {nullptr};

    wxTextCtrl* outputFolder_ {nullptr};
    wxButton* browseBtn_ {nullptr};

    // Data: keep full paths separate from displayed basenames
    wxArrayString batchFiles_;
    wxArrayString textureFiles_;
    std::map<wxString, ImageSettings> perImageSettings_;

    friend class WxFileDropTarget;

public:
    // Crop helpers
    // Returns width/height ratio derived from canvas dims; 0.0 if not set
    double getCropAspectRatio() const;
};

class WxFileDropTarget : public wxFileDropTarget
{
public:
    enum class Target { Batch, Textures };
    explicit WxFileDropTarget(WxControlPanel* owner, Target t) : owner_(owner), target_(t) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override;
private:
    WxControlPanel* owner_;
    Target target_ {Target::Batch};
};
