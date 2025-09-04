#pragma once
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/filepicker.h>
#include <vector>
#include <map>
#include <wx/dnd.h>

#include "models/ImageSettings.hpp" // from extend_canvas_ui/src

// Custom event declarations
wxDECLARE_EVENT(wxEVT_WXUI_SETTINGS_CHANGED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WXUI_PROCESS_REQUESTED, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_WXUI_BATCH_ITEM_SELECTED, wxCommandEvent);

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

private:
    void BuildUI();
    void WireEvents();
    void AddFiles(const wxArrayString& paths);
    void UpdateProcessEnabled();
    void EnsureDefaultOutputFolder();

    // Controls
    wxListBox* list_ {nullptr};
    wxButton* addBtn_ {nullptr};
    wxButton* clearBtn_ {nullptr};
    wxGauge* progress_ {nullptr};

    wxSpinCtrl* width_ {nullptr};
    wxSpinCtrl* height_ {nullptr};
    wxSpinCtrl* whiteThr_ {nullptr};
    wxSpinCtrlDouble* padding_ {nullptr};
    wxSpinCtrl* blurRadius_ {nullptr};
    wxComboBox* scaleBox_ {nullptr};
    wxButton* processBtn_ {nullptr};

    wxTextCtrl* outputFolder_ {nullptr};
    wxButton* browseBtn_ {nullptr};

    // Data: keep full paths separate from displayed basenames
    wxArrayString batchFiles_;
    std::map<wxString, ImageSettings> perImageSettings_;

    friend class WxFileDropTarget;
};

class WxFileDropTarget : public wxFileDropTarget
{
public:
    explicit WxFileDropTarget(WxControlPanel* owner) : owner_(owner) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override;
private:
    WxControlPanel* owner_;
};
