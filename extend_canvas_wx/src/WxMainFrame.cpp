#include "WxMainFrame.hpp"
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include "WxControlPanel.hpp"
#include "WxPreviewPanel.hpp"
#include "extend_canvas.hpp"

wxBEGIN_EVENT_TABLE(WxMainFrame, wxFrame)
wxEND_EVENT_TABLE()

WxMainFrame::WxMainFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Extend Canvas (wxWidgets)", wxDefaultPosition, wxSize(2200, 1300))
{
    SetMinSize(wxSize(1600, 900));
    splitter_ = new wxSplitterWindow(this, wxID_ANY);
    controls_ = new WxControlPanel(splitter_);
    preview_  = new WxPreviewPanel(splitter_);
    splitter_->SplitVertically(controls_, preview_, 380);
    splitter_->SetMinimumPaneSize(200);
    // Start maximized to ensure full width at launch
    Maximize(true);

    // Wire custom events from control panel
    controls_->Bind(wxEVT_WXUI_SETTINGS_CHANGED, [this](wxCommandEvent&){
        if (!currentImagePath_.IsEmpty())
        {
            imageSettings_[currentImagePath_] = controls_->getCurrentSettings();
            preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_]);
        }
    });

    controls_->Bind(wxEVT_WXUI_BATCH_ITEM_SELECTED, [this](wxCommandEvent& ev){
        currentImagePath_ = ev.GetString();
        if (imageSettings_.count(currentImagePath_))
        {
            controls_->loadSettings(imageSettings_[currentImagePath_]);
        }
        else
        {
            imageSettings_[currentImagePath_] = controls_->getCurrentSettings();
        }
        preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_]);
    });

    controls_->Bind(wxEVT_WXUI_PROCESS_REQUESTED, [this](wxCommandEvent&){
        wxArrayString batch = controls_->getBatchFiles();
        if (batch.IsEmpty()) return;
        wxString outDir = controls_->getOutputFolder();
        if (outDir.IsEmpty()) { preview_->SetStatus("No output folder selected", true); return; }
        wxMkdir(outDir);
        int scale = controls_->getScaleFactor();
        int processed = 0, ok = 0;
        for (auto& file : batch)
        {
            preview_->SetStatus(wxString::Format("Processing %s (%d/%zu)...", wxFileName(file).GetFullName(), processed+1, batch.size()));
            ImageSettings s = imageSettings_.count(file) ? imageSettings_[file] : controls_->getCurrentSettings();
            int rw = s.width * scale;
            int rh = s.height * scale;
            int finalW = s.finalWidth > 0 ? s.finalWidth * scale : -1;
            int finalH = s.finalHeight > 0 ? s.finalHeight * scale : -1;
            bool success = extendCanvas(std::string(file.mb_str()), rw, rh, s.whiteThreshold, s.padding, finalW, finalH, s.blurRadius);
            if (success)
            {
                wxFileName inFn(file);
                wxString tempOut = inFn.GetPathWithSep() + inFn.GetName() + "_extended." + inFn.GetExt();
                wxString outName = inFn.GetName() + "_extended";
                if (scale > 1) outName += wxString::Format("_%dx", scale);
                outName += "." + inFn.GetExt();
                wxString finalPath = wxFileName(outDir, outName).GetFullPath();
                if (wxRenameFile(tempOut, finalPath)) ++ok;
            }
            ++processed;
        }
        preview_->SetStatus(wxString::Format("Processing complete: %d/%zu images processed successfully", ok, batch.size()), ok != (int)batch.size());
    });
}
