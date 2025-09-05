#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/statbmp.h>
#include <wx/timer.h>
#include "models/ImageSettings.hpp"
#include "models/ProcessingMode.hpp"
#include "models/MaskSettings.hpp"
#include <map>

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
    // Crop helpers
    void SetCropAspectRatio(double aspectWOverH); // 0.0 for Free
    bool GetCropRect(const wxString& imagePath, wxRect& out) const;
    void SetCropRect(const wxString& imagePath, const wxRect& rect);
    wxString CurrentImagePath() const { return currentImagePath_; }

private:
    void BuildUI();
    void LayoutImages();
    void ShowOverlay(const wxString& text, const wxColour& color, int durationMs = 1200);
    void OnSize(wxSizeEvent&);

    wxScrolledWindow* scroll_ {nullptr};
    wxStaticText* originalTitle_ {nullptr};
    class CropCanvas* originalCanvas_ {nullptr};
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

    // Crop state
    double cropAspect_ {0.0}; // 0 => Free
    std::map<wxString, wxRect> cropByImage_;

    wxDECLARE_EVENT_TABLE();
};

// Simple image canvas that draws the original image and a crop overlay when in Crop mode
class CropCanvas : public wxPanel
{
public:
    explicit CropCanvas(WxPreviewPanel* owner);
    void SetImage(const wxBitmap& bmp, const wxSize& origPixelSize);
    void EnableOverlay(bool enable);
    void SetCropRectImage(const wxRect& r); // in image coordinates
    wxRect GetCropRectImage() const;
    void SetAspectRatio(double aspectWOverH); // 0 => Free

protected:
    void OnPaint(wxPaintEvent&);
    void OnLeftDown(wxMouseEvent&);
    void OnLeftUp(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnLeave(wxMouseEvent&);
    void OnSize(wxSizeEvent&);

private:
    WxPreviewPanel* owner_ {nullptr};
    wxBitmap bmp_;
    wxSize origImgSize_ {0,0}; // full-res image size
    bool overlayEnabled_ {false};
    double aspect_ {0.0};
    wxRect cropImg_; // image-space

    enum DragMode { None, Move, ResizeTL, ResizeTR, ResizeBL, ResizeBR, ResizeL, ResizeR, ResizeT, ResizeB };
    DragMode drag_ {None};
    wxPoint lastMouse_ {0,0}; // panel-space

    // Helpers
    wxRect ImageToPanel(const wxRect& r) const;
    wxRect PanelToImage(const wxRect& r) const;
    wxPoint PanelToImage(const wxPoint& p) const;
    double ScaleFactor() const; // panel pixels per image pixel (uniform)
    wxPoint ImageOriginOnPanel() const; // top-left of image in panel
    DragMode HitTest(const wxPoint& p) const; // panel-space
    void ConstrainAspect(wxRect& r) const;
    void NotifyCropChanged();

    wxDECLARE_EVENT_TABLE();
};
