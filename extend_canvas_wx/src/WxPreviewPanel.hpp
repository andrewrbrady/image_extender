#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/statbmp.h>
#include <wx/timer.h>
#include <wx/geometry.h>
#include "models/ImageSettings.hpp"
#include "models/ProcessingMode.hpp"
#include "models/MaskSettings.hpp"
#include <memory>
#include <map>
#include <vector>

namespace cv { class Mat; }

class WxPreviewPanel : public wxPanel
{
public:
    explicit WxPreviewPanel(wxWindow* parent);

    void UpdatePreview(const wxString& imagePath, const ImageSettings& settings,
                       ProcessingMode mode = ProcessingMode::ExtendCanvas,
                       const MaskSettings mask = MaskSettings());
    void UpdatePreviewDevelop(const wxString& imagePath,
                              const wxString& texturePath,
                              int blendMode,
                              float opacity,
                              bool useTextureLuminance,
                              bool swapRBTexture);
    void ClearPreview();
    void SetStatus(const wxString& message, bool isError = false);
    // Crop helpers
    void SetCropAspectRatio(double aspectWOverH); // 0.0 for Free
    bool GetCropRect(const wxString& imagePath, wxRect& out) const;
    void SetCropRect(const wxString& imagePath, const wxRect& rect);
    void FitCurrentCropToMaxHeight();
    wxString CurrentImagePath() const { return currentImagePath_; }
    ProcessingMode CurrentMode() const { return currentMode_; }
    void SetSplitterCount(int n);
    void SetCollageSources(const wxArrayString& files);
    int GetCollageSlotCount() const;
    bool RenderCollage(cv::Mat& out, int scaleFactor = 1);

    // Collage interactions (used by canvas)
    void SetCollageActiveSlot(int slot);
    int GetCollageActiveSlot() const { return collageActiveSlot_; }
    std::vector<wxRect> GetCollageSlotRectsImage() const;
    int CollageSlotIndexAtPoint(const wxPoint& imagePt) const;
    void MoveActiveCollageSlot(const wxPoint2DDouble& deltaImage);
    void ScaleActiveCollageSlot(double factor, const wxPoint2DDouble& anchorImage);
    void CycleActiveCollageSlot(int direction);
    void ChangeActiveCollageSlot(int delta);

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
    ProcessingMode currentMode_ { ProcessingMode::ExtendCanvas };
    int splitterCount_ {3};

    // Crop state
    double cropAspect_ {0.0}; // 0 => Free
    std::map<wxString, wxRect> cropByImage_;

    struct CollageSlotState
    {
        int sourceIndex {-1};
        wxString imagePath;
        double scale {1.0};
        double offsetX {0.0};
        double offsetY {0.0};
    };

    wxArrayString collageSources_;
    std::vector<CollageSlotState> collageSlots_;
    int collageActiveSlot_ {-1};
    wxSize collageCanvasSize_ {1080, 1920};
    mutable std::map<wxString, std::shared_ptr<cv::Mat>> collageImageCache_;

    void EnsureCollageSlotCount(int count);
    void EnsureCollageAssignments();
    void RebuildCollageComposite();
    std::shared_ptr<cv::Mat> LoadCollageImage(const wxString& path) const;
    void ClampCollageSlot(CollageSlotState& slot, const wxRect& slotRect, const cv::Mat& img, double actualScale);
    void RefreshCollageViews();
    int CollageSlotFromPoint(const wxPoint& imgPt) const;

    wxDECLARE_EVENT_TABLE();
};

// Simple image canvas that draws the original image and a crop overlay when in Crop mode
class CropCanvas : public wxPanel
{
public:
    explicit CropCanvas(WxPreviewPanel* owner);
    void SetImage(const wxBitmap& bmp, const wxSize& origPixelSize);
    void EnableOverlay(bool enable);
    void SetCollageMode(bool enable);
    void SetCropRectImage(const wxRect& r); // in image coordinates
    wxRect GetCropRectImage() const;
    void SetAspectRatio(double aspectWOverH); // 0 => Free
    void SetGuides(int cols, int rows); // 0 => none

protected:
    void OnPaint(wxPaintEvent&);
    void OnLeftDown(wxMouseEvent&);
    void OnLeftUp(wxMouseEvent&);
    void OnLeftDClick(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnLeave(wxMouseEvent&);
    void OnSize(wxSizeEvent&);
    void OnMouseWheel(wxMouseEvent&);
    void OnKeyDown(wxKeyEvent&);

private:
    WxPreviewPanel* owner_ {nullptr};
    wxBitmap bmp_;
    wxSize origImgSize_ {0,0}; // full-res image size
    bool overlayEnabled_ {false};
    bool collageMode_ {false};
    bool collageDragging_ {false};
    wxPoint2DDouble collageLastImg_ {0.0, 0.0};
    double aspect_ {0.0};
    wxRect cropImg_; // image-space
    int guideCols_ {0};
    int guideRows_ {0};
    // Double-click detection fallback
    wxLongLong lastClickMs_ {0};
    wxPoint lastClickPt_ {0,0};

    enum DragMode { None, Move, ResizeTL, ResizeTR, ResizeBL, ResizeBR, ResizeL, ResizeR, ResizeT, ResizeB };
    DragMode drag_ {None};
    wxPoint lastMouse_ {0,0}; // panel-space

    // Helpers
    wxRect ImageToPanel(const wxRect& r) const;
    wxRect PanelToImage(const wxRect& r) const;
    wxPoint PanelToImage(const wxPoint& p) const;
    wxPoint2DDouble PanelToImageD(const wxPoint& p) const;
    double ScaleFactor() const; // panel pixels per image pixel (uniform)
    wxPoint ImageOriginOnPanel() const; // top-left of image in panel
    DragMode HitTest(const wxPoint& p) const; // panel-space
    void ConstrainAspect(wxRect& r) const;
    void NotifyCropChanged();
    void FitToMaxHeight();

    wxDECLARE_EVENT_TABLE();
};
