#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/statbmp.h>
#include <wx/timer.h>
#include "models/ImageSettings.hpp"
#include "models/ProcessingMode.hpp"
#include "models/MaskSettings.hpp"

namespace cv { class Mat; }

class WxPreviewPanel : public wxPanel
{
public:
    explicit WxPreviewPanel(wxWindow* parent);

    void UpdatePreview(const wxString& imagePath, const ImageSettings& settings,
                       ProcessingMode mode = ProcessingMode::ExtendCanvas,
                       const MaskSettings mask = MaskSettings());
    void ClearPreview();
    void SetStatus(const wxString& message, bool isError = false);

private:
    void BuildUI();
    void LayoutImages();
    void ShowOverlay(const wxString& text, const wxColour& color, int durationMs = 1200);
    void OnSize(wxSizeEvent&);

    wxScrolledWindow* scroll_ {nullptr};
    wxStaticText* originalTitle_ {nullptr};
    wxStaticBitmap* originalBmp_ {nullptr};
    wxStaticText* resultTitle_ {nullptr};
    wxStaticBitmap* resultBmp_ {nullptr};
    wxStaticText* overlay_ {nullptr};
    wxTimer overlayHideTimer_;

    wxBitmap originalCache_;
    wxBitmap resultCache_;
    // Keep full-resolution images for high-quality display rescaling
    cv::Mat* originalMat_ {nullptr};
    cv::Mat* resultMat_ {nullptr};
    wxString currentImagePath_;
    wxString lastResultPath_;

    wxDECLARE_EVENT_TABLE();
};
