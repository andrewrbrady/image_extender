#include "WxPreviewPanel.hpp"
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/statline.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
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
    originalCanvas_ = new CropCanvas(this);
    originalCanvas_->SetMinSize(wxSize(400, 400));
    oSizer->Add(originalTitle_, 0, wxALL, 4);
    oSizer->Add(originalCanvas_, 1, wxEXPAND | wxALL, 4);
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
    // Fit images within each column's visible width and overall height
    wxSize client = scroll_->GetClientSize();
    int gutters = 60; // approximate padding + gaps
    int availWPerPanel = std::max(120, (client.x - gutters) / 2);
    int availH = std::max(120, client.y - 60);

    auto scaleMatToFit = [&](const cv::Mat& bgr){
        if (bgr.empty()) return wxBitmap();
        double sx = double(availWPerPanel) / std::max(1, bgr.cols);
        double sy = double(availH) / std::max(1, bgr.rows);
        double s = std::min(sx, sy);
        int nw = std::max(1, int(bgr.cols * s));
        int nh = std::max(1, int(bgr.rows * s));
        cv::Mat resized; cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LANCZOS4);
        cv::Mat rgb; cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        size_t size = static_cast<size_t>(rgb.cols) * static_cast<size_t>(rgb.rows) * 3;
        unsigned char* buf = new unsigned char[size];
        std::memcpy(buf, rgb.data, size);
        wxImage wi(rgb.cols, rgb.rows, buf, false);
        return wxBitmap(wi);
    };

    if (originalMat_) {
        wxBitmap bm = scaleMatToFit(*originalMat_);
        originalCanvas_->SetImage(bm, wxSize(originalMat_->cols, originalMat_->rows));
    } else if (originalCache_.IsOk()) {
        // Cache already in wxBitmap form; but we still want to ensure it fits the panel width/height
        // Convert cache back to Mat-like scaling via wxImage to keep path uniform
        wxImage img = originalCache_.ConvertToImage();
        cv::Mat bgr(img.GetHeight(), img.GetWidth(), CV_8UC3, img.GetData());
        // wxImage stores RGB; convert to BGR view without copying (safe enough for sizing)
        cv::Mat bgrCopy; cv::cvtColor(bgr, bgrCopy, cv::COLOR_RGB2BGR);
        wxBitmap bm = scaleMatToFit(bgrCopy);
        originalCanvas_->SetImage(bm, wxSize(originalCache_.GetWidth(), originalCache_.GetHeight()));
    }

    if (resultMat_) {
        resultBmp_->SetBitmap(scaleMatToFit(*resultMat_));
    } else if (resultCache_.IsOk()) {
        // Keep cached bitmap, but if it's oversized it will still be displayed; re-scaling ensures fit
        wxImage img = resultCache_.ConvertToImage();
        cv::Mat bgr(img.GetHeight(), img.GetWidth(), CV_8UC3, img.GetData());
        cv::Mat bgrCopy; cv::cvtColor(bgr, bgrCopy, cv::COLOR_RGB2BGR);
        resultBmp_->SetBitmap(scaleMatToFit(bgrCopy));
    }

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
    currentMode_ = mode;
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
    if (originalCanvas_) {
        originalCanvas_->EnableOverlay(mode == ProcessingMode::Crop || mode == ProcessingMode::Splitter);
        if (mode == ProcessingMode::Splitter) originalCanvas_->SetGuides(std::max(2, splitterCount_), 1); else originalCanvas_->SetGuides(0, 0);
    }

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

    // Splitter preview: show N-panel split guidelines and scaled crop
    if (mode == ProcessingMode::Splitter)
    {
        // Initialize crop rect if missing for this image, using triple-panel aspect
        if (!cropByImage_.count(imagePath))
        {
            int W = img.cols, H = img.rows;
            int cw = int(W * 0.9 + 0.5);
            int ch = int(H * 0.9 + 0.5);
            if (cropAspect_ > 0.0)
            {
                double want = cropAspect_;
                if (double(cw)/double(ch) > want) { cw = int(ch * want + 0.5); } else { ch = int(cw / want + 0.5); }
            }
            if (cw > W) cw = W; if (ch > H) ch = H;
            int cx = (W - cw)/2; int cy = (H - ch)/2;
            cropByImage_[imagePath] = wxRect(cx, cy, cw, ch);
        }
        // Update canvas with overlay + guides
        originalCanvas_->EnableOverlay(true);
        originalCanvas_->SetAspectRatio(cropAspect_);
        originalCanvas_->SetCropRectImage(cropByImage_[imagePath]);

        // Build a preview image as the cropped area resized to 3*W x H with guide lines
        wxRect cr = cropByImage_[imagePath];
        int n = std::max(2, splitterCount_);
        // Align width to multiple of n to ensure equal-width panels
        int alignedW = std::max(n, (cr.width / n) * n);
        if (alignedW != cr.width)
        {
            int dx = (cr.width - alignedW) / 2;
            cr.x += dx; cr.width = alignedW;
        }
        cv::Rect roi(cr.x, cr.y, cr.width, cr.height);
        roi &= cv::Rect(0,0,img.cols, img.rows);
        cv::Mat cropped = img(roi).clone();
        int panelW = settings.width > 0 ? settings.width : std::max(1, cropped.cols / n);
        int panelH = settings.height > 0 ? settings.height : cropped.rows;
        int previewW = std::max(n, panelW * n);
        int previewH = std::max(1, panelH);
        cv::Mat resized; cv::resize(cropped, resized, cv::Size(previewW, previewH), 0,0, cv::INTER_LANCZOS4);
        // Guidelines
        for (int i = 1; i < n; ++i)
        {
            int x = panelW * i;
            cv::line(resized, cv::Point(x, 0), cv::Point(x, resized.rows-1), cv::Scalar(40,220,90), 2);
        }

        if (resultMat_) { delete resultMat_; resultMat_ = nullptr; }
        resultMat_ = new cv::Mat(resized.clone());
        auto toWxBitmap2 = [](const cv::Mat& bgr){
            cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
            const size_t size = static_cast<size_t>(rgb.cols) * static_cast<size_t>(rgb.rows) * 3;
            unsigned char* buf = new unsigned char[size];
            std::memcpy(buf, rgb.data, size);
            wxImage wi(rgb.cols, rgb.rows, buf, false);
            return wxBitmap(wi);
        };
        resultCache_ = toWxBitmap2(resized);
        resultTitle_->SetLabel("Split Preview (" + wxString::Format("%dx%d per panel", panelW, panelH) + ")");
        LayoutImages();
        ShowOverlay(wxString::FromUTF8("Preview"), wxColour(100,100,100), 400);
        return;
    }

    if (mode == ProcessingMode::Crop)
    {
        // Initialize crop rect if missing for this image
        if (!cropByImage_.count(imagePath))
        {
            // Default to centered rect covering 80% of the shorter side, honoring aspect if set
            int W = img.cols, H = img.rows;
            int cw = int(W * 0.8 + 0.5);
            int ch = int(H * 0.8 + 0.5);
            if (cropAspect_ > 0.0)
            {
                double want = cropAspect_;
                // Adjust width/height to match aspect while fitting inside image
                if (double(cw)/double(ch) > want) { cw = int(ch * want + 0.5); } else { ch = int(cw / want + 0.5); }
            }
            if (cw > W) cw = W; if (ch > H) ch = H;
            int cx = (W - cw)/2; int cy = (H - ch)/2;
            cropByImage_[imagePath] = wxRect(cx, cy, cw, ch);
        }
        // Update canvas
        originalCanvas_->EnableOverlay(true);
        originalCanvas_->SetAspectRatio(cropAspect_);
        originalCanvas_->SetCropRectImage(cropByImage_[imagePath]);

        // Build cropped result and optionally scale to requested output size
        wxRect cr = cropByImage_[imagePath];
        cv::Rect roi(cr.x, cr.y, cr.width, cr.height);
        roi &= cv::Rect(0,0,img.cols, img.rows);
        cv::Mat cropped = img(roi).clone();
        auto applyFinalResize = [](const cv::Mat& canvas, int reqW, int reqH){
            if (reqW <= 0 || reqH <= 0) return canvas.clone();
            double sx = double(reqW)/canvas.cols, sy = double(reqH)/canvas.rows; double s = std::min(sx, sy);
            int nw = std::max(1, int(canvas.cols * s + 0.5)); int nh = std::max(1, int(canvas.rows * s + 0.5));
            cv::Mat resized; cv::resize(canvas, resized, cv::Size(nw, nh), 0,0, cv::INTER_LANCZOS4);
            cv::Mat final(reqH, reqW, canvas.type(), cv::Scalar(255,255,255));
            int x = (reqW - nw)/2; int y = (reqH - nh)/2; resized.copyTo(final(cv::Rect(x,y,nw,nh))); return final;
        };
        int desiredW = settings.width > 0 ? settings.width : cropped.cols;
        int desiredH = settings.height > 0 ? settings.height : cropped.rows;
        cv::Mat result = applyFinalResize(cropped, desiredW, desiredH);

        // Convert to wxBitmap (deep copy) and store full-res mat
        if (resultMat_) { delete resultMat_; resultMat_ = nullptr; }
        resultMat_ = new cv::Mat(result.clone());
        auto toWxBitmap = [](const cv::Mat& bgr){
            cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
            const size_t size = static_cast<size_t>(rgb.cols) * static_cast<size_t>(rgb.rows) * 3;
            unsigned char* buf = new unsigned char[size];
            std::memcpy(buf, rgb.data, size);
            wxImage wi(rgb.cols, rgb.rows, buf, false);
            return wxBitmap(wi);
        };
        resultCache_ = toWxBitmap(result);
        resultTitle_->SetLabel("Crop Preview (" + wxString::Format("%dx%d", resultCache_.GetWidth(), resultCache_.GetHeight()) + ")");
        LayoutImages();
        ShowOverlay(wxString::FromUTF8("Preview"), wxColour(100, 100, 100), 400);
        return;
    }

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
    if (originalCanvas_) originalCanvas_->SetImage(wxNullBitmap, wxSize(0,0));
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

void WxPreviewPanel::SetCropAspectRatio(double aspectWOverH)
{
    cropAspect_ = aspectWOverH;
    if (!currentImagePath_.IsEmpty() && cropByImage_.count(currentImagePath_) && originalMat_ && !originalMat_->empty())
    {
        // Adjust current rect to maintain center while applying aspect
        wxRect r = cropByImage_[currentImagePath_];
        if (cropAspect_ > 0.0)
        {
            int cx = r.GetX() + r.GetWidth()/2;
            int cy = r.GetY() + r.GetHeight()/2;
            int w = r.GetWidth(); int h = r.GetHeight();
            double want = cropAspect_;
            if (double(w)/double(h) > want) { w = int(h * want + 0.5); } else { h = int(w / want + 0.5); }
            int imgW = originalMat_->cols, imgH = originalMat_->rows;
            w = std::max(4, std::min(w, imgW));
            h = std::max(4, std::min(h, imgH));
            int x = std::max(0, cx - w/2); int y = std::max(0, cy - h/2);
            if (x + w > imgW) x = imgW - w;
            if (y + h > imgH) y = imgH - h;
            cropByImage_[currentImagePath_] = wxRect(x,y,w,h);
        }
        if (originalCanvas_) { originalCanvas_->SetAspectRatio(cropAspect_); originalCanvas_->SetCropRectImage(cropByImage_[currentImagePath_]); }
    }
}

bool WxPreviewPanel::GetCropRect(const wxString& imagePath, wxRect& out) const
{
    auto it = cropByImage_.find(imagePath);
    if (it == cropByImage_.end()) return false;
    out = it->second; return true;
}

void WxPreviewPanel::SetCropRect(const wxString& imagePath, const wxRect& rect)
{
    cropByImage_[imagePath] = rect;
}

void WxPreviewPanel::FitCurrentCropToMaxHeight()
{
    if (!originalMat_ || originalMat_->empty() || cropAspect_ <= 0.0 || currentImagePath_.IsEmpty()) return;
    int imgW = originalMat_->cols; int imgH = originalMat_->rows;
    int targetH = imgH;
    int targetW = int(targetH * cropAspect_ + 0.5);
    if (targetW > imgW) { targetW = imgW; targetH = int(targetW / cropAspect_ + 0.5); }
    int x = 0; // default left
    wxRect cur = cropByImage_[currentImagePath_];
    // keep current center X if possible
    int cx = cur.GetX() + cur.GetWidth()/2;
    x = std::clamp(cx - targetW/2, 0, std::max(0, imgW - targetW));
    int y = (targetH == imgH) ? 0 : (imgH - targetH)/2;
    cropByImage_[currentImagePath_] = wxRect(x, y, targetW, targetH);
}

// ===== CropCanvas implementation =====

wxBEGIN_EVENT_TABLE(CropCanvas, wxPanel)
    EVT_PAINT(CropCanvas::OnPaint)
    EVT_LEFT_DOWN(CropCanvas::OnLeftDown)
    EVT_LEFT_UP(CropCanvas::OnLeftUp)
    EVT_LEFT_DCLICK(CropCanvas::OnLeftDClick)
    EVT_MOTION(CropCanvas::OnMotion)
    EVT_LEAVE_WINDOW(CropCanvas::OnLeave)
    EVT_SIZE(CropCanvas::OnSize)
wxEND_EVENT_TABLE()

CropCanvas::CropCanvas(WxPreviewPanel* owner) : wxPanel(owner), owner_(owner)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCURSOR_ARROW);
}

void CropCanvas::SetImage(const wxBitmap& bmp, const wxSize& origPixelSize)
{
    bmp_ = bmp;
    origImgSize_ = origPixelSize;
    Refresh();
}

void CropCanvas::EnableOverlay(bool enable)
{
    overlayEnabled_ = enable;
    Refresh();
}

void CropCanvas::SetCropRectImage(const wxRect& r)
{
    // Clamp incoming rect to image bounds to avoid out-of-bounds overlays
    wxRect clamped = r;
    int imgW = origImgSize_.GetWidth();
    int imgH = origImgSize_.GetHeight();
    if (imgW > 0 && imgH > 0)
    {
        if (clamped.width > imgW) clamped.width = imgW;
        if (clamped.height > imgH) clamped.height = imgH;
        if (clamped.x < 0) clamped.x = 0;
        if (clamped.y < 0) clamped.y = 0;
        if (clamped.x + clamped.width > imgW) clamped.x = imgW - clamped.width;
        if (clamped.y + clamped.height > imgH) clamped.y = imgH - clamped.height;
    }
    cropImg_ = clamped;
    Refresh();
}

wxRect CropCanvas::GetCropRectImage() const { return cropImg_; }

void CropCanvas::SetAspectRatio(double aspectWOverH)
{
    aspect_ = aspectWOverH;
}

void CropCanvas::SetGuides(int cols, int rows)
{
    guideCols_ = std::max(0, cols);
    guideRows_ = std::max(0, rows);
    Refresh();
}

void CropCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();
    if (!bmp_.IsOk()) return;
    // Draw image centered
    wxPoint origin = ImageOriginOnPanel();
    dc.DrawBitmap(bmp_, origin);
    if (overlayEnabled_ && cropImg_.GetWidth() > 0 && cropImg_.GetHeight() > 0)
    {
        wxRect pr = ImageToPanel(cropImg_);
        // Dim outside
        wxRect client(wxPoint(0,0), GetClientSize());
        wxBrush dimBrush(wxColour(0,0,0,80));
        dc.SetBrush(dimBrush);
        dc.SetPen(*wxTRANSPARENT_PEN);
        // Four rects around crop
        dc.DrawRectangle(0,0, client.width, pr.y);
        dc.DrawRectangle(0, pr.y, pr.x, pr.height);
        dc.DrawRectangle(pr.x + pr.width, pr.y, client.width - (pr.x + pr.width), pr.height);
        dc.DrawRectangle(0, pr.y + pr.height, client.width, client.height - (pr.y + pr.height));

        // Draw crop rect border
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(0, 200, 80), 2));
        dc.DrawRectangle(pr);

        // Draw guidelines (e.g., thirds for splitter)
        if (guideCols_ > 1 || guideRows_ > 1)
        {
            dc.SetPen(wxPen(wxColour(40,220,90), 1, wxPENSTYLE_SOLID));
            // Vertical guides
            for (int i = 1; i < guideCols_; ++i)
            {
                int x = pr.x + (pr.width * i) / guideCols_;
                dc.DrawLine(x, pr.y, x, pr.y + pr.height);
            }
            // Horizontal guides
            for (int j = 1; j < guideRows_; ++j)
            {
                int y = pr.y + (pr.height * j) / guideRows_;
                dc.DrawLine(pr.x, y, pr.x + pr.width, y);
            }
        }

        // Draw handles (larger for easier selection)
        const int hs = 12;
        auto drawHandle = [&](int x, int y){ dc.SetBrush(wxBrush(wxColour(255,255,255))); dc.SetPen(wxPen(wxColour(0,0,0))); dc.DrawRectangle(x - hs/2, y - hs/2, hs, hs); };
        drawHandle(pr.x, pr.y);
        drawHandle(pr.x + pr.width, pr.y);
        drawHandle(pr.x, pr.y + pr.height);
        drawHandle(pr.x + pr.width, pr.y + pr.height);
    }
}

void CropCanvas::OnLeftDown(wxMouseEvent& e)
{
    if (!overlayEnabled_) return;
    SetFocus();
    if (!HasCapture()) CaptureMouse();
    // Manual double-click detection fallback
    wxLongLong now = wxGetLocalTimeMillis();
    if (lastClickMs_ != 0 && (now - lastClickMs_).ToLong() <= 500) {
        wxPoint p = e.GetPosition();
        if (ImageToPanel(cropImg_).Contains(p) && (std::abs(p.x - lastClickPt_.x) <= 12) && (std::abs(p.y - lastClickPt_.y) <= 12)) {
            // Trigger fit-to-height
            FitToMaxHeight();
            lastClickMs_ = 0;
            e.Skip();
            return;
        }
    }
    lastClickMs_ = wxGetLocalTimeMillis();
    lastClickPt_ = e.GetPosition();
    lastMouse_ = e.GetPosition();
    drag_ = HitTest(lastMouse_);
    if (drag_ == None && ImageToPanel(cropImg_).Contains(lastMouse_)) drag_ = Move;
    // Allow default processing so double-click events are generated by wx
    e.Skip();
}

void CropCanvas::OnLeftUp(wxMouseEvent&)
{
    if (HasCapture()) ReleaseMouse();
    if (drag_ != None) NotifyCropChanged();
    drag_ = None;
    SetCursor(wxCURSOR_ARROW);
}

void CropCanvas::OnLeftDClick(wxMouseEvent& e)
{
    if (!overlayEnabled_) return;
    wxPoint p = e.GetPosition();
    if (!ImageToPanel(cropImg_).Contains(p)) return;
    if (HasCapture()) ReleaseMouse();
    drag_ = None;
    SetCursor(wxCURSOR_ARROW);
    FitToMaxHeight();
}

void CropCanvas::OnMotion(wxMouseEvent& e)
{
    if (!overlayEnabled_) return;
    wxPoint p = e.GetPosition();
    if (drag_ == None) {
        // If user is dragging with button down but we missed down event, initialize drag from hover state
        if (e.Dragging() && e.LeftIsDown()) {
            drag_ = HitTest(p);
            if (drag_ == None && ImageToPanel(cropImg_).Contains(p)) drag_ = Move;
            if (drag_ != None) {
                if (!HasCapture()) CaptureMouse();
                lastMouse_ = p; // prevent initial jump; first delta == 0
                // Set appropriate cursor immediately
                if (drag_ == ResizeTL || drag_ == ResizeBR) SetCursor(wxCURSOR_SIZENWSE);
                else if (drag_ == ResizeTR || drag_ == ResizeBL) SetCursor(wxCURSOR_SIZENESW);
                else if (drag_ == ResizeL || drag_ == ResizeR) SetCursor(wxCURSOR_SIZEWE);
                else if (drag_ == ResizeT || drag_ == ResizeB) SetCursor(wxCURSOR_SIZENS);
                else SetCursor(wxCURSOR_SIZING);
                return; // wait for next motion before applying changes
            }
        }
        // Hover cursors for affordance
        DragMode hit = HitTest(p);
        if (hit == ResizeTL || hit == ResizeBR) SetCursor(wxCURSOR_SIZENWSE);
        else if (hit == ResizeTR || hit == ResizeBL) SetCursor(wxCURSOR_SIZENESW);
        else if (hit == ResizeL || hit == ResizeR) SetCursor(wxCURSOR_SIZEWE);
        else if (hit == ResizeT || hit == ResizeB) SetCursor(wxCURSOR_SIZENS);
        else if (ImageToPanel(cropImg_).Contains(p)) SetCursor(wxCURSOR_SIZING);
        else SetCursor(wxCURSOR_ARROW);
        return;
    }
    wxPoint delta = p - lastMouse_;
    lastMouse_ = p;

    // Work in image space for precision
    double s = ScaleFactor(); if (s <= 0) s = 1.0;
    wxPoint dImg(int(delta.x / s + 0.5), int(delta.y / s + 0.5));
    if (dImg.x == 0 && dImg.y == 0) {
        // No movement yet; ignore to avoid zero-delta aspect corrections
        return;
    }
    wxRect prev = cropImg_;
    wxRect r = prev;
    const int minSize = 10;
    if (drag_ == Move)
    {
        r.Offset(dImg);
    }
    else
    {
        // Corner/edge resizing
        if (drag_ == ResizeTL || drag_ == ResizeL || drag_ == ResizeBL) { r.x += dImg.x; r.width -= dImg.x; }
        if (drag_ == ResizeTR || drag_ == ResizeR || drag_ == ResizeBR) { r.width += dImg.x; }
        if (drag_ == ResizeTL || drag_ == ResizeT || drag_ == ResizeTR) { r.y += dImg.y; r.height -= dImg.y; }
        if (drag_ == ResizeBL || drag_ == ResizeB || drag_ == ResizeBR) { r.height += dImg.y; }
        // Enforce aspect with appropriate anchor for the dragged edge/corner
        if (aspect_ > 0.0)
        {
            double want = aspect_;
            int w = std::max(minSize, r.width);
            int h = std::max(minSize, r.height);
            if (double(w)/double(h) > want) h = int(w / want + 0.5); else w = int(h * want + 0.5);
            switch (drag_)
            {
                case ResizeTL: {
                    int ax = prev.x + prev.width; int ay = prev.y + prev.height; r.width = w; r.height = h; r.x = ax - r.width; r.y = ay - r.height; break; }
                case ResizeTR: {
                    int ax = prev.x; int ay = prev.y + prev.height; r.width = w; r.height = h; r.x = ax; r.y = ay - r.height; break; }
                case ResizeBL: {
                    int ax = prev.x + prev.width; int ay = prev.y; r.width = w; r.height = h; r.x = ax - r.width; r.y = ay; break; }
                case ResizeBR: {
                    int ax = prev.x; int ay = prev.y; r.width = w; r.height = h; r.x = ax; r.y = ay; break; }
                case ResizeL: {
                    int ax = prev.x + prev.width; r.width = w; r.x = ax - r.width; int cy = prev.y + prev.height/2; r.height = int(r.width / want + 0.5); r.y = cy - r.height/2; break; }
                case ResizeR: {
                    int ax = prev.x; r.width = w; r.x = ax; int cy = prev.y + prev.height/2; r.height = int(r.width / want + 0.5); r.y = cy - r.height/2; break; }
                case ResizeT: {
                    int ay = prev.y + prev.height; r.height = h; r.y = ay - r.height; int cx = prev.x + prev.width/2; r.width = int(r.height * want + 0.5); r.x = cx - r.width/2; break; }
                case ResizeB: {
                    int ay = prev.y; r.height = h; r.y = ay; int cx = prev.x + prev.width/2; r.width = int(r.height * want + 0.5); r.x = cx - r.width/2; break; }
                default: break;
            }
        }
        if (r.width < minSize) r.width = minSize; if (r.height < minSize) r.height = minSize;
        // Ensure the resized rect cannot exceed the image dimensions. If it does, shrink it while
        // preserving aspect (if any) and anchor relative to the dragged edge/corner similar to above.
        int imgW = origImgSize_.GetWidth();
        int imgH = origImgSize_.GetHeight();
        if (imgW > 0 && imgH > 0 && (r.width > imgW || r.height > imgH))
        {
            int w = std::min(r.width, imgW);
            int h = std::min(r.height, imgH);
            if (aspect_ > 0.0)
            {
                double want = aspect_;
                // Fit w/h to image while maintaining aspect
                // Start from the limited side and compute the other from aspect, then recheck
                if (double(w)/double(h) > want) { w = int(h * want + 0.5); } else { h = int(w / want + 0.5); }
                if (w > imgW) { w = imgW; h = int(w / want + 0.5); }
                if (h > imgH) { h = imgH; w = int(h * want + 0.5); }
                // Re-anchor using the same logic as above
                switch (drag_)
                {
                    case ResizeTL: { int ax = prev.x + prev.width; int ay = prev.y + prev.height; r.width = w; r.height = h; r.x = ax - r.width; r.y = ay - r.height; break; }
                    case ResizeTR: { int ax = prev.x;               int ay = prev.y + prev.height; r.width = w; r.height = h; r.x = ax;              r.y = ay - r.height; break; }
                    case ResizeBL: { int ax = prev.x + prev.width; int ay = prev.y;               r.width = w; r.height = h; r.x = ax - r.width; r.y = ay;               break; }
                    case ResizeBR: { int ax = prev.x;               int ay = prev.y;               r.width = w; r.height = h; r.x = ax;              r.y = ay;               break; }
                    case ResizeL:  { int ax = prev.x + prev.width; r.width = w; r.x = ax - r.width; int cy = prev.y + prev.height/2; r.height = int(r.width / want + 0.5); r.y = cy - r.height/2; break; }
                    case ResizeR:  { int ax = prev.x;               r.width = w; r.x = ax;              int cy = prev.y + prev.height/2; r.height = int(r.width / want + 0.5); r.y = cy - r.height/2; break; }
                    case ResizeT:  { int ay = prev.y + prev.height; r.height = h; r.y = ay - r.height; int cx = prev.x + prev.width/2; r.width  = int(r.height * want + 0.5); r.x = cx - r.width/2; break; }
                    case ResizeB:  { int ay = prev.y;               r.height = h; r.y = ay;              int cx = prev.x + prev.width/2; r.width  = int(r.height * want + 0.5); r.x = cx - r.width/2; break; }
                    default: break;
                }
            }
            else
            {
                // Free aspect: just cap size but keep the modified edges
                int dx = r.width  - w;
                int dy = r.height - h;
                r.width = w; r.height = h;
                // If we reduced width/height, adjust position to keep the opposite edge fixed
                if (dx > 0)
                {
                    if (drag_ == ResizeL || drag_ == ResizeTL || drag_ == ResizeBL) r.x += dx;
                }
                if (dy > 0)
                {
                    if (drag_ == ResizeT || drag_ == ResizeTL || drag_ == ResizeTR) r.y += dy;
                }
            }
        }
    }
    // Final clamp to image bounds (position only; size is already limited above)
    if (r.x < 0) r.x = 0; if (r.y < 0) r.y = 0;
    int imgW = origImgSize_.GetWidth();
    int imgH = origImgSize_.GetHeight();
    if (imgW > 0 && r.x + r.width > imgW) r.x = imgW - r.width;
    if (imgH > 0 && r.y + r.height > imgH) r.y = imgH - r.height;
    cropImg_ = r;
    Refresh();
    // Live notify for realtime preview
    NotifyCropChanged();
}

void CropCanvas::OnLeave(wxMouseEvent&)
{
    SetCursor(wxCURSOR_ARROW);
}

void CropCanvas::OnSize(wxSizeEvent& event)
{
    Refresh();
    event.Skip();
}

wxRect CropCanvas::ImageToPanel(const wxRect& r) const
{
    double s = ScaleFactor();
    wxPoint o = ImageOriginOnPanel();
    return wxRect(o.x + int(r.x * s + 0.5), o.y + int(r.y * s + 0.5), int(r.width * s + 0.5), int(r.height * s + 0.5));
}

wxRect CropCanvas::PanelToImage(const wxRect& r) const
{
    double s = ScaleFactor(); if (s <= 0) s = 1.0;
    wxPoint o = ImageOriginOnPanel();
    return wxRect(int((r.x - o.x)/s + 0.5), int((r.y - o.y)/s + 0.5), int(r.width/s + 0.5), int(r.height/s + 0.5));
}

wxPoint CropCanvas::PanelToImage(const wxPoint& p) const
{
    double s = ScaleFactor(); if (s <= 0) s = 1.0;
    wxPoint o = ImageOriginOnPanel();
    return wxPoint(int((p.x - o.x)/s + 0.5), int((p.y - o.y)/s + 0.5));
}

double CropCanvas::ScaleFactor() const
{
    if (!bmp_.IsOk() || origImgSize_.GetWidth() <= 0 || origImgSize_.GetHeight() <= 0) return 1.0;
    // bmp_ is already scaled to fit height by parent; compute its scale vs original image
    // However, SetImage gives bmp scaled by parent; we can just compute s = bmp_.GetWidth() / origImgSize_.width
    double sx = double(bmp_.GetWidth()) / double(origImgSize_.GetWidth());
    double sy = double(bmp_.GetHeight()) / double(origImgSize_.GetHeight());
    return std::min(sx, sy);
}

wxPoint CropCanvas::ImageOriginOnPanel() const
{
    // Center the bitmap within panel
    wxSize cs = GetClientSize();
    int x = (cs.x - bmp_.GetWidth())/2; if (x < 0) x = 0;
    int y = (cs.y - bmp_.GetHeight())/2; if (y < 0) y = 0;
    return wxPoint(x,y);
}

CropCanvas::DragMode CropCanvas::HitTest(const wxPoint& p) const
{
    wxRect pr = ImageToPanel(cropImg_);
    const int tol = 12; // larger tolerance for easier selection
    auto nearPt = [&](int x, int y){ return std::abs(p.x - x) <= tol && std::abs(p.y - y) <= tol; };
    if (nearPt(pr.x, pr.y)) return ResizeTL;
    if (nearPt(pr.x + pr.width, pr.y)) return ResizeTR;
    if (nearPt(pr.x, pr.y + pr.height)) return ResizeBL;
    if (nearPt(pr.x + pr.width, pr.y + pr.height)) return ResizeBR;
    // edges
    if (std::abs(p.x - pr.x) <= tol && p.y >= pr.y && p.y <= pr.y + pr.height) return ResizeL;
    if (std::abs(p.x - (pr.x + pr.width)) <= tol && p.y >= pr.y && p.y <= pr.y + pr.height) return ResizeR;
    if (std::abs(p.y - pr.y) <= tol && p.x >= pr.x && p.x <= pr.x + pr.width) return ResizeT;
    if (std::abs(p.y - (pr.y + pr.height)) <= tol && p.x >= pr.x && p.x <= pr.x + pr.width) return ResizeB;
    return None;
}

void CropCanvas::ConstrainAspect(wxRect& r) const
{
    if (aspect_ <= 0.0) return;
    // Maintain anchor at top-left for simplicity after ops that reduced width/height
    if (r.width <= 0 || r.height <= 0) return;
    double want = aspect_;
    if (double(r.width)/double(r.height) > want)
    {
        r.width = int(r.height * want + 0.5);
    }
    else
    {
        r.height = int(r.width / want + 0.5);
    }
}

void CropCanvas::NotifyCropChanged()
{
    if (!owner_) return;
    if (!owner_->CurrentImagePath().IsEmpty())
        owner_->SetCropRect(owner_->CurrentImagePath(), cropImg_);
    // Trigger preview recompute for current mode (Crop or Splitter)
    if (!owner_->CurrentImagePath().IsEmpty())
    {
        ImageSettings s; s.width = 0; s.height = 0; s.whiteThreshold = -1; s.padding = 0.0; s.blurRadius = 0;
        owner_->UpdatePreview(owner_->CurrentImagePath(), s, owner_->CurrentMode(), MaskSettings());
    }
}

void WxPreviewPanel::SetSplitterCount(int n)
{
    splitterCount_ = std::max(2, n);
    if (originalCanvas_ && currentMode_ == ProcessingMode::Splitter)
    {
        originalCanvas_->SetGuides(splitterCount_, 1);
        originalCanvas_->Refresh();
    }
}

void CropCanvas::FitToMaxHeight()
{
    int imgW = origImgSize_.GetWidth();
    int imgH = origImgSize_.GetHeight();
    if (imgW <= 0 || imgH <= 0) return;
    double ar = aspect_ > 0.0 ? aspect_ : (cropImg_.GetHeight() > 0 ? double(cropImg_.GetWidth())/double(cropImg_.GetHeight()) : 0.0);
    if (ar <= 0.0) return;
    int targetH = imgH;
    int targetW = int(targetH * ar + 0.5);
    if (targetW > imgW) { targetW = imgW; targetH = int(targetW / ar + 0.5); }
    // Keep current center X if possible
    int cx = cropImg_.GetX() + cropImg_.GetWidth()/2;
    int x = std::clamp(cx - targetW/2, 0, std::max(0, imgW - targetW));
    int y = (targetH == imgH) ? 0 : (imgH - targetH)/2;
    x = std::clamp(x, 0, std::max(0, imgW - targetW));
    y = std::clamp(y, 0, std::max(0, imgH - targetH));
    cropImg_ = wxRect(x, y, targetW, targetH);
    Refresh();
    NotifyCropChanged();
}
