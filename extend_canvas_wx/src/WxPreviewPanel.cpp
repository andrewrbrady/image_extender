#include "WxPreviewPanel.hpp"
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/statline.h>
#include <wx/dcclient.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cstring>

using namespace cv;

wxBEGIN_EVENT_TABLE(WxPreviewPanel, wxPanel)
    EVT_SIZE(WxPreviewPanel::OnSize)
wxEND_EVENT_TABLE()

WxPreviewPanel::WxPreviewPanel(wxWindow* parent)
    : wxPanel(parent), overlayHideTimer_(this)
{
    BuildUI();
    overlayHideTimer_.Bind(wxEVT_TIMER, [this](wxTimerEvent&){ overlay_->Hide(); });
}

void WxPreviewPanel::BuildUI()
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    scroll_ = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxHSCROLL);
    scroll_->SetScrollRate(10, 10);
    auto* content = new wxPanel(scroll_);
    auto* grid = new wxGridSizer(1, 2, 8, 8);

    // Original
    auto* origPanel = new wxPanel(content);
    auto* oSizer = new wxBoxSizer(wxVERTICAL);
    originalTitle_ = new wxStaticText(origPanel, wxID_ANY, "Original");
    originalBmp_ = new wxStaticBitmap(origPanel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(400, 400));
    oSizer->Add(originalTitle_, 0, wxALL, 4);
    oSizer->Add(originalBmp_, 1, wxEXPAND | wxALL, 4);
    origPanel->SetSizer(oSizer);

    // Result
    auto* resPanel = new wxPanel(content);
    auto* rSizer = new wxBoxSizer(wxVERTICAL);
    resultTitle_ = new wxStaticText(resPanel, wxID_ANY, "Result");
    resultBmp_ = new wxStaticBitmap(resPanel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(400, 400));
    rSizer->Add(resultTitle_, 0, wxALL, 4);
    rSizer->Add(resultBmp_, 1, wxEXPAND | wxALL, 4);
    resPanel->SetSizer(rSizer);

    grid->Add(origPanel, 1, wxEXPAND | wxALL, 8);
    grid->Add(resPanel, 1, wxEXPAND | wxALL, 8);

    content->SetSizer(grid);
    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(content, 1, wxEXPAND | wxALL, 8);
    scroll_->SetSizer(scrollSizer);
    scroll_->FitInside();

    root->Add(scroll_, 1, wxEXPAND);

    // Overlay label centered (no opacity animation, simple timer)
    overlay_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    overlay_->Hide();
    overlay_->SetBackgroundColour(wxColour(0,0,0,0));

    SetSizer(root);
}

void WxPreviewPanel::LayoutImages()
{
    // Scale bitmaps to fit height of visible area roughly
    int availH = scroll_->GetClientSize().y - 60;
    if (availH < 100) availH = 100;
    auto scaleMatToH = [&](const cv::Mat& bgr){
        if (bgr.empty()) return wxBitmap();
        double scale = double(availH) / bgr.rows;
        int nw = std::max(1, int(bgr.cols * scale));
        int nh = std::max(1, int(bgr.rows * scale));
        cv::Mat resized; cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LANCZOS4);
        // BGR->RGB and deep copy to wx
        cv::Mat rgb; cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        size_t size = static_cast<size_t>(rgb.cols) * static_cast<size_t>(rgb.rows) * 3;
        unsigned char* buf = new unsigned char[size];
        std::memcpy(buf, rgb.data, size);
        wxImage wi(rgb.cols, rgb.rows, buf, false);
        return wxBitmap(wi);
    };

    if (originalMat_) originalBmp_->SetBitmap(scaleMatToH(*originalMat_));
    else if (originalCache_.IsOk()) originalBmp_->SetBitmap(originalCache_);
    if (resultMat_) resultBmp_->SetBitmap(scaleMatToH(*resultMat_));
    else if (resultCache_.IsOk()) resultBmp_->SetBitmap(resultCache_);

    // Position overlay to cover full panel
    overlay_->SetSize(GetClientSize());
    overlay_->Move(0, 0);

    // Ensure layouts refresh
    scroll_->FitInside();
    scroll_->Layout();
    this->Layout();
    this->Refresh();
}

void WxPreviewPanel::OnSize(wxSizeEvent&)
{
    LayoutImages();
}

void WxPreviewPanel::UpdatePreview(const wxString& imagePath, const ImageSettings& settings, ProcessingMode mode, const MaskSettings mask)
{
    currentImagePath_ = imagePath;
    // Load original (for preview-only) and build processed result entirely in memory
    cv::Mat img = cv::imread(std::string(imagePath.mb_str()));
    if (img.empty()) { SetStatus("Failed to load image", true); return; }

    // Helper: deep-copy convert Mat(BGR) -> wxBitmap (RGB)
    auto toWxBitmap = [](const cv::Mat& bgr){
        cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        const size_t size = static_cast<size_t>(rgb.cols) * static_cast<size_t>(rgb.rows) * 3;
        unsigned char* buf = new unsigned char[size];
        std::memcpy(buf, rgb.data, size);
        wxImage wi(rgb.cols, rgb.rows, buf, false /*wxImage owns and frees buf*/);
        return wxBitmap(wi);
    };

    // Convert to wxBitmap for original
    // Keep full-res mats for high-quality display scaling
    if (originalMat_) { delete originalMat_; originalMat_ = nullptr; }
    originalMat_ = new cv::Mat(img.clone());
    originalCache_ = toWxBitmap(img);
    originalTitle_->SetLabel("Original (" + wxString::Format("%dx%d", originalCache_.GetWidth(), originalCache_.GetHeight()) + ")");

    // If in Vehicle Mask mode, build a mask preview using provided settings and return
    if (mode == ProcessingMode::VehicleMask)
    {
        // Prepare preview bitmaps
        if (originalMat_) { delete originalMat_; originalMat_ = nullptr; }
        originalMat_ = new cv::Mat(img.clone());
        originalCache_ = toWxBitmap(img);
        originalTitle_->SetLabel("Original (" + wxString::Format("%dx%d", originalCache_.GetWidth(), originalCache_.GetHeight()) + ")");
        // Compute mask via shared logic for consistency
        extern bool computeVehicleMaskMat(const cv::Mat&, cv::Mat&, const MaskSettings&);
        cv::Mat maskImg; computeVehicleMaskMat(img, maskImg, mask);
        cv::Mat mask3; cv::cvtColor(maskImg, mask3, cv::COLOR_GRAY2BGR);
        if (resultMat_) { delete resultMat_; resultMat_ = nullptr; }
        resultMat_ = new cv::Mat(mask3.clone());
        resultCache_ = toWxBitmap(mask3);
        resultTitle_->SetLabel("Mask Preview");
        LayoutImages();
        ShowOverlay(wxString::FromUTF8("Preview"), wxColour(100, 100, 100), 600);
        return;
    }

    // Helper lambdas replicating core logic (no disk I/O)
    auto centerSampleThreshold = [](const Mat& im, int stripeH = 20, int stripeW = 40){
        int cx = im.cols / 2;
        int w  = std::min({ stripeW, std::max(1, cx - 1), std::max(1, im.cols - cx - 1) });
        int h  = std::min(stripeH, std::max(1, im.rows / 10));
        Rect topR(cx - w, 0, 2*w + 1, h);
        Rect botR(cx - w, im.rows - h, 2*w + 1, h);
        Mat gt, gb; cvtColor(im(topR), gt, COLOR_BGR2GRAY); cvtColor(im(botR), gb, COLOR_BGR2GRAY);
        double mt = mean(gt)[0], mb = mean(gb)[0];
        int thr = int(std::min(mt, mb) - 5.0); thr = std::clamp(thr, 180, 250); return thr;
    };
    auto findForegroundBounds = [](const Mat& im, int& top, int& bot, int whiteThr){
        Mat mask; inRange(im, Scalar(whiteThr,whiteThr,whiteThr), Scalar(255,255,255), mask);
        bitwise_not(mask, mask);
        reduce(mask, mask, 1, REDUCE_MAX, CV_8U);
        top = bot = -1; for (int r=0;r<mask.rows;++r){ if (mask.at<uchar>(r,0)){ if (top==-1) top=r; bot=r; } }
        return top != -1;
    };
    auto findForegroundBoundsX = [](const Mat& im, int& left, int& right, int whiteThr){
        Mat mask; inRange(im, Scalar(whiteThr,whiteThr,whiteThr), Scalar(255,255,255), mask);
        bitwise_not(mask, mask);
        reduce(mask, mask, 0, REDUCE_MAX, CV_8U);
        left = right = -1; for (int c=0;c<mask.cols;++c){ if (mask.at<uchar>(0,c)){ if (left==-1) left=c; right=c; } }
        return left != -1;
    };
    auto makeStrip = [](const Mat& src, int newH, int W){
        if (newH <= 0) return Mat();
        if (!src.empty()){ Mat dst; resize(src, dst, Size(W,newH), 0,0, INTER_AREA); return dst; }
        return Mat(newH, W, CV_8UC3, Scalar(255,255,255));
    };
    auto applyFinalResize = [](const Mat& canvas, int reqW, int reqH){
        if (reqW <= 0 || reqH <= 0) return canvas.clone();
        double sx = double(reqW)/canvas.cols, sy = double(reqH)/canvas.rows; double s = std::min(sx, sy);
        int nw = std::max(1, int(canvas.cols * s + 0.5)); int nh = std::max(1, int(canvas.rows * s + 0.5));
        Mat resized; resize(canvas, resized, Size(nw, nh), 0,0, INTER_LANCZOS4);
        Mat final(reqH, reqW, canvas.type(), Scalar(255,255,255));
        int x = (reqW - nw)/2; int y = (reqH - nh)/2; resized.copyTo(final(Rect(x,y,nw,nh))); return final;
    };
    auto blendSeam = [](Mat& img, int seamX, int overlap){
        if (overlap <= 0) return;
        if (seamX - overlap < 0 || seamX + overlap > img.cols) return;
        Mat left = img(Rect(seamX - overlap, 0, overlap, img.rows)).clone();
        Mat right = img(Rect(seamX, 0, overlap, img.rows)).clone();
        for (int i=0;i<overlap;++i){ double a = (i+1.0)/(overlap+1.0); Mat dst = img(Rect(seamX - overlap + i, 0, 1, img.rows)); addWeighted(right.col(i), a, left.col(i), 1.0 - a, 0.0, dst); }
    };

    int thr = (settings.whiteThreshold >= 0 && settings.whiteThreshold <= 255)
              ? settings.whiteThreshold : centerSampleThreshold(img);
    int fgTop, fgBot; if (!findForegroundBounds(img, fgTop, fgBot, thr)) { SetStatus("Foreground not found", true); return; }
    int carH = fgBot - fgTop + 1;
    int pad  = int(carH * settings.padding + 0.5);
    int cropTop = std::max(0, fgTop - pad);
    int cropBot = std::min(img.rows - 1, fgBot + pad);
    Mat carReg = img.rowRange(cropTop, cropBot + 1);

    int desiredW = settings.width > 0 ? settings.width : img.cols;
    int desiredH = settings.height > 0 ? settings.height : img.rows;
    int W = img.cols;

    // Horizontal crop metrics (for building left/right extension strips later)
    int fgLeft = 0, fgRight = img.cols - 1; findForegroundBoundsX(img, fgLeft, fgRight, thr);
    int carW = std::max(1, fgRight - fgLeft + 1);
    int padX = int(carW * settings.padding + 0.5);
    int cropLeft = std::max(0, fgLeft - padX);
    int cropRight = std::min(img.cols - 1, fgRight + padX);

    Mat result;
    if (desiredH <= carReg.rows)
    {
        int yOff = (carReg.rows - desiredH) / 2; result = carReg.rowRange(yOff, yOff + desiredH).clone();
        if (false && desiredW > result.cols)
        {
            // Extend horizontally instead of scaling up
            int extraW = desiredW - result.cols; int leftW = extraW/2; int rightW = extraW - leftW;
            auto makeStripW = [](const Mat& src, int H, int newW){
                if (newW <= 0) return Mat();
                if (!src.empty()){ Mat dst; resize(src, dst, Size(newW, H), 0,0, INTER_AREA); return dst; }
                return Mat(H, newW, CV_8UC3, Scalar(255,255,255));
            };
            Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : Mat();
            Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : Mat();
            Mat leftStrip = makeStripW(leftSrc, result.rows, leftW);
            Mat rightStrip = makeStripW(rightSrc, result.rows, rightW);
            if (settings.blurRadius > 0)
            { int k = std::max(1, settings.blurRadius*2+1); if(!leftStrip.empty()) GaussianBlur(leftStrip, leftStrip, Size(k,k), 0); if(!rightStrip.empty()) GaussianBlur(rightStrip, rightStrip, Size(k,k), 0); }
            Mat wide(desiredH, desiredW, img.type()); int x = 0;
            if (!leftStrip.empty()) { leftStrip.copyTo(wide(Rect(x,0,leftStrip.cols,leftStrip.rows))); x += leftStrip.cols; }
            result.copyTo(wide(Rect(x,0,result.cols,result.rows))); x += result.cols;
            if (!rightStrip.empty()) { rightStrip.copyTo(wide(Rect(x,0,rightStrip.cols,rightStrip.rows))); }
            if (!leftStrip.empty()) { int seamX = leftStrip.cols; int ov = std::min({24, leftStrip.cols, wide.cols - seamX}); blendSeam(wide, seamX, ov); }
            if (!rightStrip.empty()) { int seamX = wide.cols - rightStrip.cols; int ov = std::min({24, rightStrip.cols, seamX}); blendSeam(wide, seamX, ov); }
            result = wide;
        }
        if (desiredW != result.cols)
        {
            double scale = double(desiredW) / result.cols; int sh = int(result.rows * scale + 0.5);
            resize(result, result, Size(desiredW, sh), 0,0, INTER_LANCZOS4);
            if (sh != desiredH)
            {
                if (sh > desiredH)
                { int y = (sh - desiredH)/2; result = result.rowRange(y, y+desiredH).clone(); }
                else
                { Mat ext(desiredH, desiredW, result.type(), Scalar(255,255,255)); int y = (desiredH - sh)/2; result.copyTo(ext.rowRange(y, y+sh)); result = ext; }
            }
        }
    }
    else
    {
        int extra = desiredH - carReg.rows; int topH = extra/2; int botH = extra - topH; int targetW = W;
        Mat scaledCarReg = carReg;
        Mat scaledTopSrc, scaledBotSrc;
        // Pre-scale to target width whenever it differs
        if (desiredW != W)
        {
            double sc = double(desiredW)/W; int sh = int(carReg.rows * sc + 0.5);
            resize(carReg, scaledCarReg, Size(desiredW, sh), 0,0, INTER_LANCZOS4);
            Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
            Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
            if (!topSrc.empty()){ int sth = int(topSrc.rows * sc + 0.5); resize(topSrc, scaledTopSrc, Size(desiredW, sth), 0,0, INTER_LANCZOS4); }
            if (!botSrc.empty()){ int sbh = int(botSrc.rows * sc + 0.5); resize(botSrc, scaledBotSrc, Size(desiredW, sbh), 0,0, INTER_LANCZOS4); }
            extra = desiredH - scaledCarReg.rows; topH = extra/2; botH = extra - topH;
            targetW = desiredW;
        }
        else
        {
            scaledTopSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
            scaledBotSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
            targetW = W;
        }
        Mat topStrip = makeStrip(scaledTopSrc, topH, targetW);
        Mat botStrip = makeStrip(scaledBotSrc, botH, targetW);
        if (settings.blurRadius > 0)
        { int k = std::max(1, settings.blurRadius*2+1); if(!topStrip.empty()) GaussianBlur(topStrip, topStrip, Size(k,k), 0); if(!botStrip.empty()) GaussianBlur(botStrip, botStrip, Size(k,k), 0); }
        result.create(desiredH, targetW, img.type()); int y=0; if(!topStrip.empty()){ topStrip.copyTo(result.rowRange(y, y+topStrip.rows)); y+=topStrip.rows; }
        scaledCarReg.copyTo(result.rowRange(y, y+scaledCarReg.rows)); y+=scaledCarReg.rows; if(!botStrip.empty()) botStrip.copyTo(result.rowRange(y, y+botStrip.rows));

        // No horizontal extension
        if (false && desiredW > result.cols)
        {
            int extraW = desiredW - result.cols; int leftW = extraW/2; int rightW = extraW - leftW;
            auto makeStripW = [](const Mat& src, int H, int newW){
                if (newW <= 0) return Mat();
                if (!src.empty()){ Mat dst; resize(src, dst, Size(newW, H), 0,0, INTER_AREA); return dst; }
                return Mat(H, newW, CV_8UC3, Scalar(255,255,255));
            };
            Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : Mat();
            Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : Mat();
            Mat leftStrip = makeStripW(leftSrc, result.rows, leftW);
            Mat rightStrip = makeStripW(rightSrc, result.rows, rightW);
            if (settings.blurRadius > 0)
            { int k = std::max(1, settings.blurRadius*2+1); if(!leftStrip.empty()) GaussianBlur(leftStrip, leftStrip, Size(k,k), 0); if(!rightStrip.empty()) GaussianBlur(rightStrip, rightStrip, Size(k,k), 0); }
            Mat wide(desiredH, desiredW, img.type()); int x = 0;
            if (!leftStrip.empty()) { leftStrip.copyTo(wide(Rect(x,0,leftStrip.cols,leftStrip.rows))); x += leftStrip.cols; }
            result.copyTo(wide(Rect(x,0,result.cols,result.rows))); x += result.cols;
            if (!rightStrip.empty()) { rightStrip.copyTo(wide(Rect(x,0,rightStrip.cols,rightStrip.rows))); }
            // seam blend no-op (feature disabled)
            result = wide;
        }
    }
    // Final resize if provided
    result = applyFinalResize(result, settings.finalWidth, settings.finalHeight);

    // Convert to wxBitmap (deep copy) and store full-res mat
    if (resultMat_) { delete resultMat_; resultMat_ = nullptr; }
    resultMat_ = new cv::Mat(result.clone());
    resultCache_ = toWxBitmap(result);
    resultTitle_->SetLabel("Result (" + wxString::Format("%dx%d", resultCache_.GetWidth(), resultCache_.GetHeight()) + ")");
    LayoutImages();
    ShowOverlay(wxString::FromUTF8("✓"), wxColour(0, 200, 80), 800);
}

void WxPreviewPanel::ClearPreview()
{
    lastResultPath_.clear();
    currentImagePath_.clear();
    originalCache_ = wxBitmap();
    resultCache_ = wxBitmap();
    originalBmp_->SetBitmap(wxNullBitmap);
    resultBmp_->SetBitmap(wxNullBitmap);
    originalTitle_->SetLabel("Original");
    resultTitle_->SetLabel("Result");
}

void WxPreviewPanel::SetStatus(const wxString& message, bool isError)
{
    if (!isError) return;
    ShowOverlay(message, wxColour(220, 50, 47), 1600);
}

void WxPreviewPanel::ShowOverlay(const wxString& text, const wxColour& color, int durationMs)
{
    overlay_->SetLabel(text);
    overlay_->SetForegroundColour(color);
    overlay_->SetFont(wxFont(wxFontInfo(32).Bold()));
    overlay_->SetSize(GetClientSize());
    overlay_->CentreOnParent();
    overlay_->Show();
    overlayHideTimer_.StartOnce(durationMs);
}
