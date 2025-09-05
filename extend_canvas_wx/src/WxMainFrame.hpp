#pragma once
#include <wx/wx.h>
#include <wx/splitter.h>
#include <map>
#include "models/ImageSettings.hpp"

class WxControlPanel;
class WxPreviewPanel;

class WxMainFrame : public wxFrame
{
public:
    explicit WxMainFrame(wxWindow* parent);

private:
    wxSplitterWindow* splitter_ {nullptr};
    WxControlPanel* controls_ {nullptr};
    WxPreviewPanel* preview_ {nullptr};

    // Data
    std::map<wxString, ImageSettings> imageSettings_;
    wxString currentImagePath_;

    wxDECLARE_EVENT_TABLE();

private:
    void OnQuit(wxCommandEvent&);
};
