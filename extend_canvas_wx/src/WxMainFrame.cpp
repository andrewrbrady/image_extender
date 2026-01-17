#include "WxMainFrame.hpp"
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include "WxControlPanel.hpp"
#include "WxPreviewPanel.hpp"
#include "extend_canvas.hpp"
#include "auto_fit_vehicle.hpp"
#include "vehicle_mask.hpp"
#include <opencv2/opencv.hpp>
#include <array>
#include <random>
#include <cstdlib>

wxBEGIN_EVENT_TABLE(WxMainFrame, wxFrame)
wxEND_EVENT_TABLE()

WxMainFrame::WxMainFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Extend Canvas (wxWidgets)", wxDefaultPosition, wxSize(2200, 1300))
{
    SetMinSize(wxSize(1600, 900));
    // Create a basic menu bar to enable common macOS shortcuts
    // Standard IDs ensure Cmd+Q (Quit) and Cmd+H (Hide) work automatically on macOS
    auto* menuBar = new wxMenuBar();
    auto* fileMenu = new wxMenu();
#ifdef __WXMAC__
    fileMenu->Append(wxID_OSX_HIDE);
    fileMenu->Append(wxID_OSX_HIDEOTHERS);
    fileMenu->AppendSeparator();
#endif
    fileMenu->Append(wxID_EXIT);
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);
    Bind(wxEVT_MENU, &WxMainFrame::OnQuit, this, wxID_EXIT);
    splitter_ = new wxSplitterWindow(this, wxID_ANY);
    controls_ = new WxControlPanel(splitter_);
    preview_  = new WxPreviewPanel(splitter_);
    splitter_->SplitVertically(controls_, preview_, 380);
    splitter_->SetMinimumPaneSize(200);
    // Start maximized to ensure full width at launch
    Maximize(true);

    // Wire custom events from control panel
    controls_->Bind(wxEVT_WXUI_SETTINGS_CHANGED, [this](wxCommandEvent&){
        preview_->SetCollageSources(controls_->getBatchFiles());
        if (controls_->getMode() == ProcessingMode::SplitCollage && currentImagePath_.IsEmpty() && !controls_->getBatchFiles().IsEmpty())
        {
            currentImagePath_ = controls_->getBatchFiles()[0];
        }
        if (!currentImagePath_.IsEmpty())
        {
            imageSettings_[currentImagePath_] = controls_->getCurrentSettings();
            // Push crop aspect before updating preview so overlay is consistent
            preview_->SetCropAspectRatio(controls_->getCropAspectRatio());
            preview_->SetSplitterCount(controls_->getSplitterCount());
            if (controls_->getMode() == ProcessingMode::FilmDevelop)
            {
                wxString texPath = controls_->getSelectedTexturePath();
                if (!texPath.IsEmpty()) preview_->UpdatePreviewDevelop(
                    currentImagePath_, texPath,
                    controls_->getDevelopBlendMode(), controls_->getDevelopOpacity(),
                    controls_->getUseTextureLuminance(), controls_->getSwapRB());
                else preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_], ProcessingMode::ExtendCanvas, controls_->getMaskSettings());
            }
            else
            {
                preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_], controls_->getMode(), controls_->getMaskSettings());
            }
        }
    });

    controls_->Bind(wxEVT_WXUI_BATCH_ITEM_SELECTED, [this](wxCommandEvent& ev){
        currentImagePath_ = ev.GetString();
        preview_->SetCollageSources(controls_->getBatchFiles());
        if (imageSettings_.count(currentImagePath_))
        {
            controls_->loadSettings(imageSettings_[currentImagePath_]);
        }
        else
        {
            // Initialize defaults from current UI values, but keep extend mode non-destructive
            ImageSettings defaults = controls_->getCurrentSettings();
            if (controls_->getMode() == ProcessingMode::ExtendCanvas)
            {
                defaults.width = 0;
                defaults.height = 0;
                defaults.finalWidth = -1;
                defaults.finalHeight = -1;
            }
            imageSettings_[currentImagePath_] = defaults;
            controls_->loadSettings(defaults);
        }
        preview_->SetCropAspectRatio(controls_->getCropAspectRatio());
        preview_->SetSplitterCount(controls_->getSplitterCount());
        if (controls_->getMode() == ProcessingMode::FilmDevelop)
        {
            wxString texPath = controls_->getSelectedTexturePath();
            if (!texPath.IsEmpty()) preview_->UpdatePreviewDevelop(
                currentImagePath_, texPath,
                controls_->getDevelopBlendMode(), controls_->getDevelopOpacity(),
                controls_->getUseTextureLuminance(), controls_->getSwapRB());
            else preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_], ProcessingMode::ExtendCanvas, controls_->getMaskSettings());
        }
        else
        {
            preview_->UpdatePreview(currentImagePath_, imageSettings_[currentImagePath_], controls_->getMode(), controls_->getMaskSettings());
        }
    });

    controls_->Bind(wxEVT_WXUI_PROCESS_REQUESTED, [this](wxCommandEvent&){
        wxArrayString batch = controls_->getBatchFiles();
        if (batch.IsEmpty()) return;
        wxString outDir = controls_->getOutputFolder();
        if (outDir.IsEmpty()) { preview_->SetStatus("No output folder selected", true); return; }
        if (controls_->getMode() == ProcessingMode::SplitCollage)
        {
            preview_->SetCollageSources(batch);
            wxFileName outFn(outDir, "");
            if (!outFn.DirExists()) {
                if (!outFn.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
                    preview_->SetStatus("Failed to create output folder", true);
                    return;
                }
            }
            cv::Mat collage;
            int scale = std::max(1, controls_->getScaleFactor());
            if (!preview_->RenderCollage(collage, scale))
            {
                preview_->SetStatus("Collage preview not ready", true);
                return;
            }
            int splits = std::max(2, preview_->GetCollageSlotCount());
            wxString baseName = wxString::Format("collage_%dsplit", splits);
            int attempt = 1;
            wxFileName candidate;
            wxString fileName;
            do {
                if (attempt == 1) fileName = baseName + ".png";
                else fileName = wxString::Format("%s_%d.png", baseName, attempt);
                candidate.Assign(outDir, fileName);
                ++attempt;
            } while (candidate.FileExists());

            bool ok = cv::imwrite(std::string(candidate.GetFullPath().mb_str()), collage);
            if (ok)
            {
                preview_->SetStatus(wxString::Format("Saved collage to %s", candidate.GetFullName()), false);
            }
            else
            {
                preview_->SetStatus("Failed to save collage", true);
            }
            return;
        }
        // Create full directory path robustly (handles spaces and intermediate dirs)
        wxFileName outFn(outDir, "");
        if (!outFn.DirExists()) {
            if (!outFn.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
                preview_->SetStatus("Failed to create output folder", true);
                return;
            }
        }
        int scale = controls_->getScaleFactor();
        int processed = 0, ok = 0;
        for (auto& file : batch)
        {
            preview_->SetStatus(wxString::Format("Processing %s (%d/%zu)...", wxFileName(file).GetFullName(), processed+1, batch.size()));
            const bool haveSettings = imageSettings_.count(file) > 0;
            ImageSettings s = haveSettings ? imageSettings_[file] : controls_->getCurrentSettings();
            if (!haveSettings && controls_->getMode() == ProcessingMode::ExtendCanvas)
            {
                s.width = 0;
                s.height = 0;
                s.finalWidth = -1;
                s.finalHeight = -1;
                imageSettings_[file] = s;
            }
            int rw = s.width * scale;
            int rh = s.height * scale;
            int finalW = s.finalWidth > 0 ? s.finalWidth * scale : -1;
            int finalH = s.finalHeight > 0 ? s.finalHeight * scale : -1;
            bool success = false;
            if (controls_->getMode() == ProcessingMode::ExtendCanvas)
            {
                success = extendCanvas(std::string(file.mb_str()), rw, rh, s.whiteThreshold, s.padding, finalW, finalH, s.blurRadius);
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
            }
            else if (controls_->getMode() == ProcessingMode::AutoFitVehicle)
            {
                wxFileName inFn(file);
                wxString tempOut = inFn.GetPathWithSep() + inFn.GetName() + "_autofit." + inFn.GetExt();
                // Call shared auto-fit, which writes sibling file with _autofit suffix
                success = autoFitVehicle(std::string(file.mb_str()), rw, rh, s, controls_->getMaskSettings());
                if (success)
                {
                    wxString outName = inFn.GetName() + "_autofit";
                    if (scale > 1) outName += wxString::Format("_%dx", scale);
                    outName += "." + inFn.GetExt();
                    wxString finalPath = wxFileName(outDir, outName).GetFullPath();
                    if (wxRenameFile(tempOut, finalPath)) ++ok;
                }
            }
            else if (controls_->getMode() == ProcessingMode::VehicleMask)
            {
                // Vehicle Mask mode
                wxFileName inFn(file);
                wxString outName = inFn.GetName() + "_mask.png"; // always PNG
                wxString finalPath = wxFileName(outDir, outName).GetFullPath();
                // Ensure output directory exists (already ensured above)
                // Call shared mask generator (implemented to call an external SAM2 script)
                success = generateVehicleMask(std::string(file.mb_str()), std::string(finalPath.mb_str()), controls_->getMaskSettings());
                if (success) ++ok;
            }
            else if (controls_->getMode() == ProcessingMode::Crop)
            {
                // Crop mode: use preview's crop rect if available; else center based on aspect
                wxRect cr;
                bool haveRect = preview_->GetCropRect(file, cr);
                double ar = controls_->getCropAspectRatio();
                // Load image
                cv::Mat img = cv::imread(std::string(file.mb_str()));
                if (!img.empty())
                {
                    if (!haveRect)
                    {
                        int W = img.cols, H = img.rows;
                        int cw = int(W * 0.8 + 0.5), ch = int(H * 0.8 + 0.5);
                        if (ar > 0.0)
                        {
                            if (double(cw)/double(ch) > ar) cw = int(ch * ar + 0.5); else ch = int(cw / ar + 0.5);
                        }
                        int cx = (W - cw)/2, cy = (H - ch)/2;
                        cr = wxRect(cx, cy, cw, ch);
                    }
                    cv::Rect roi(cr.x, cr.y, cr.width, cr.height);
                    roi &= cv::Rect(0,0,img.cols, img.rows);
                    cv::Mat cropped = img(roi).clone();
                    int rw = std::max(1, s.width * scale);
                    int rh = std::max(1, s.height * scale);
                    // Resize to requested output size if provided
                    if (s.width > 0 && s.height > 0)
                    {
                        cv::Mat resized; cv::resize(cropped, resized, cv::Size(rw, rh), 0,0, cv::INTER_LANCZOS4);
                        cropped = resized;
                    }
                    wxFileName inFn(file);
                    wxString outName = inFn.GetName() + "_crop." + inFn.GetExt();
                    wxString finalPath = wxFileName(outDir, outName).GetFullPath();
                    success = cv::imwrite(std::string(finalPath.mb_str()), cropped);
                    if (success) ++ok;
                }
            }
            else if (controls_->getMode() == ProcessingMode::Splitter)
            {
                // Splitter: use preview's crop rect to split into 3 equal vertical panels
                int splitsN = std::max(2, controls_->getSplitterCount());
                wxRect cr;
                bool haveRect = preview_->GetCropRect(file, cr);
                double ar = controls_->getCropAspectRatio();
                cv::Mat img = cv::imread(std::string(file.mb_str()));
                if (!img.empty())
                {
                    if (!haveRect)
                    {
                        int W = img.cols, H = img.rows;
                        int cw = int(W * 0.9 + 0.5), ch = int(H * 0.9 + 0.5);
                        if (ar > 0.0)
                        {
                            if (double(cw)/double(ch) > ar) cw = int(ch * ar + 0.5); else ch = int(cw / ar + 0.5);
                        }
                        int cx = (W - cw)/2, cy = (H - ch)/2;
                        cr = wxRect(cx, cy, cw, ch);
                    }
                    cv::Rect roi(cr.x, cr.y, cr.width, cr.height);
                    roi &= cv::Rect(0,0,img.cols, img.rows);
                    cv::Mat cropped = img(roi).clone();
                    // Compute per-panel slice rects in cropped coords
                    int totalW = cropped.cols;
                    int baseW = std::max(1, totalW / splitsN);
                    int rem = totalW - baseW * splitsN;
                    int rw = std::max(1, s.width * scale);
                    int rh = std::max(1, s.height * scale);
                    wxFileName inFn(file);
                    bool allOk = true;
                    int x = 0;
                    for (int i=0;i<splitsN;++i)
                    {
                        int w = baseW + ((i == splitsN - 1) ? rem : 0);
                        cv::Rect r(x, 0, w, cropped.rows);
                        r &= cv::Rect(0,0,cropped.cols, cropped.rows);
                        cv::Mat tile = cropped(r).clone();
                        x += w;
                        if (s.width > 0 && s.height > 0)
                        {
                            cv::Mat resized; cv::resize(tile, resized, cv::Size(rw, rh), 0,0, cv::INTER_LANCZOS4);
                            tile = resized;
                        }
                        wxString outName = inFn.GetName() + wxString::Format("_split_%d.", i+1) + inFn.GetExt();
                        wxString finalPath = wxFileName(outDir, outName).GetFullPath();
                        bool okTile = cv::imwrite(std::string(finalPath.mb_str()), tile);
                        allOk = allOk && okTile;
                    }
                    success = allOk;
                    if (success) ++ok;
                }
            }
            ++processed;
        }
        preview_->SetStatus(wxString::Format("Processing complete: %d/%zu images processed successfully", ok, batch.size()), ok != (int)batch.size());
    });

    // Film Develop: apply random texture with random blend + opacity to current image
    controls_->Bind(wxEVT_WXUI_DEVELOP_REQUESTED, [this](wxCommandEvent&){
        if (currentImagePath_.IsEmpty()) { preview_->SetStatus("No image selected", true); return; }
        wxArrayString textures = controls_->getTextureFiles();
        if (textures.IsEmpty()) { preview_->SetStatus("No textures added", true); return; }
        wxString outDir = controls_->getOutputFolder();
        if (outDir.IsEmpty()) { preview_->SetStatus("No output folder selected", true); return; }
        // Ensure output folder
        wxFileName outFn(outDir, "");
        if (!outFn.DirExists()) {
            if (!outFn.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) { preview_->SetStatus("Failed to create output folder", true); return; }
        }

        // Choose parameters either from controls or randomized per toggle
        wxString texPath = controls_->getSelectedTexturePath();
        int mode = controls_->getDevelopBlendMode();
        float opacity = controls_->getDevelopOpacity();
        if (controls_->getRandomizeOnDevelop())
        {
            // Randomize using control panel helper to keep UI in sync
            controls_->RandomizeDevelopParams();
            texPath = controls_->getSelectedTexturePath();
            mode = controls_->getDevelopBlendMode();
            opacity = controls_->getDevelopOpacity();
        }

        // Load base and texture
        cv::Mat base = cv::imread(std::string(currentImagePath_.mb_str()), cv::IMREAD_COLOR);
        if (base.empty()) { preview_->SetStatus("Failed to load base image", true); return; }
        cv::Mat tex = cv::imread(std::string(texPath.mb_str()), cv::IMREAD_UNCHANGED);
        if (tex.empty()) { preview_->SetStatus("Failed to load texture", true); return; }

        // Resize texture to base size
        if (tex.cols != base.cols || tex.rows != base.rows) {
            cv::resize(tex, tex, cv::Size(base.cols, base.rows), 0, 0, cv::INTER_LANCZOS4);
        }

        // Keep blending in BGR (OpenCV native)
        cv::Mat texBGR, alpha;
        switch (tex.channels()) {
            case 4: {
                std::vector<cv::Mat> ch; cv::split(tex, ch); // BGRA
                // Try BGRA->BGR first; correct later if RGBA is detected by heuristic
                cv::cvtColor(tex, texBGR, cv::COLOR_BGRA2BGR);
                // Base alpha from A channel
                const double aInv = (ch[3].depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
                cv::Mat alphaFromA; ch[3].convertTo(alphaFromA, CV_32F, aInv);
                if (controls_->getUseTextureLuminance()) {
                    // Use luminance as alpha, neutralize color to gray, and respect original A
                    cv::Mat gray; cv::cvtColor(texBGR, gray, cv::COLOR_BGR2GRAY);
                    const double invMaxTex = (texBGR.depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
                    cv::Mat alphaFromLuma; gray.convertTo(alphaFromLuma, CV_32F, invMaxTex);
                    alpha = alphaFromLuma.mul(alphaFromA);
                    cv::cvtColor(gray, texBGR, cv::COLOR_GRAY2BGR);
                } else {
                    alpha = alphaFromA;
                }
                break;
            }
            case 3: {
                texBGR = tex;
                if (controls_->getUseTextureLuminance()) {
                    // Derive alpha from luminance AND neutralize texture color to avoid strong tints
                    cv::Mat gray; cv::cvtColor(texBGR, gray, cv::COLOR_BGR2GRAY);
                    const double invMaxTex = (texBGR.depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
                    gray.convertTo(alpha, CV_32F, invMaxTex);
                    cv::cvtColor(gray, texBGR, cv::COLOR_GRAY2BGR);
                } else {
                    alpha = cv::Mat(base.rows, base.cols, CV_32F, cv::Scalar(1.0));
                }
                break;
            }
            case 1:
            default: {
                if (controls_->getUseTextureLuminance()) {
                    tex.convertTo(alpha, CV_32F, 1.0/255.0);
                } else {
                    alpha = cv::Mat(base.rows, base.cols, CV_32F, cv::Scalar(1.0));
                }
                cv::cvtColor(tex, texBGR, cv::COLOR_GRAY2BGR);
                break;
            }
        }
        // Apply flags similar to preview
        if (controls_->getSwapRB()) { std::vector<cv::Mat> ch; cv::split(texBGR, ch); std::swap(ch[0], ch[2]); cv::merge(ch, texBGR); }
        else {
            auto isBlueDominant = [](const cv::Mat& bgr){
                if (bgr.empty()) return false;
                cv::Mat gray; cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
                cv::Mat mask; cv::threshold(gray, mask, 32, 255, cv::THRESH_BINARY);
                if (cv::countNonZero(mask) < (bgr.rows * bgr.cols) / 200) return false;
                cv::Scalar mB = cv::mean(bgr, mask);
                double mb = mB[0], mg = mB[1], mr = mB[2];
                return mb > 1.3 * std::max(1e-6, mr) && mb > 1.3 * std::max(1e-6, mg);
            };
            bool fixed = false;
            if (tex.channels() == 4) {
                if (isBlueDominant(texBGR)) {
                    cv::Mat alt; cv::cvtColor(tex, alt, cv::COLOR_RGBA2BGR);
                    if (!isBlueDominant(alt)) { texBGR = alt; fixed = true; }
                }
            } else if (tex.channels() == 3) {
                if (isBlueDominant(texBGR)) {
                    std::vector<cv::Mat> ch; cv::split(texBGR, ch); std::swap(ch[0], ch[2]); cv::Mat alt; cv::merge(ch, alt);
                    if (!isBlueDominant(alt)) { texBGR = alt; fixed = true; }
                }
            }
            (void)fixed;
        }

        // Auto-neutralize color for black-background JPG textures when luminance is not explicitly requested
        if (!controls_->getUseTextureLuminance()) {
            cv::Mat gray; cv::cvtColor(texBGR, gray, cv::COLOR_BGR2GRAY);
            cv::Mat bgMask; cv::threshold(gray, bgMask, 16, 255, cv::THRESH_BINARY_INV);
            double bgRatio = double(cv::countNonZero(bgMask)) / double(texBGR.rows * texBGR.cols);
            cv::Mat fgMask; cv::threshold(gray, fgMask, 32, 255, cv::THRESH_BINARY);
            int fgCount = cv::countNonZero(fgMask);
            if (fgCount > (texBGR.rows * texBGR.cols) / 200) {
                cv::Scalar m = cv::mean(texBGR, fgMask);
                double mb = m[0], mg = m[1], mr = m[2];
                double spread = std::max({ std::abs(mb - mr), std::abs(mb - mg), std::abs(mg - mr) });
                if (bgRatio > 0.4 && spread > 5.0) {
                    const double invMaxTex = (texBGR.depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
                    gray.convertTo(alpha, CV_32F, invMaxTex);
                    cv::cvtColor(gray, texBGR, cv::COLOR_GRAY2BGR);
                }
            }
        }
        // When Use texture luminance is enabled, we use luminance as per-pixel alpha (set above), not as replacement color.
        // Convert to float [0,1] with proper scaling for bit depth
        cv::Mat baseF, texF;
        {
            const double invMaxBase = (base.depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
            base.convertTo(baseF, CV_32F, invMaxBase);
        }
        {
            const double invMaxTex = (texBGR.depth() == CV_16U) ? (1.0/65535.0) : (1.0/255.0);
            texBGR.convertTo(texF, CV_32F, invMaxTex);
        }

        // Blend mode implementations (explicit per-channel)
        std::vector<cv::Mat> a(3), b(3), c(3);
        cv::split(baseF, a);
        cv::split(texF, b);
        switch (mode) {
            case 0: // Multiply
                for (int i=0;i<3;++i) cv::multiply(a[i], b[i], c[i]);
                break;
            case 1: // Screen
                for (int i=0;i<3;++i) {
                    cv::Mat one = cv::Mat::ones(a[i].size(), a[i].type());
                    c[i] = one - (one - a[i]).mul(one - b[i]);
                }
                break;
            default: // Lighten
                for (int i=0;i<3;++i) c[i] = cv::max(a[i], b[i]);
                break;
        }
        cv::Mat blended; cv::merge(c, blended);

        // Effective per-pixel alpha: texture alpha * random opacity
        cv::Mat alpha3; cv::Mat alphaScaled = alpha * opacity; cv::Mat chs[] = {alphaScaled, alphaScaled, alphaScaled};
        cv::merge(chs, 3, alpha3);
        cv::Mat resultF = baseF.mul(1.0 - alpha3) + blended.mul(alpha3);
        cv::Mat result8BGR; resultF = cv::min(cv::max(resultF, 0), 1);
        resultF.convertTo(result8BGR, CV_8U, 255.0);

        // Optional debug dump when WX_DEV_DEBUG=1
        if (const char* dbg = std::getenv("WX_DEV_DEBUG"); dbg && std::string(dbg) == "1") {
            auto pTex = wxFileName(outDir, "debug_tex.jpg").GetFullPath();
            auto pAlpha = wxFileName(outDir, "debug_alpha.png").GetFullPath();
            auto pBlend = wxFileName(outDir, "debug_blended.jpg").GetFullPath();
            cv::Mat alpha8; cv::Mat alphaClamped = cv::min(cv::max(alpha, 0), 1); alphaClamped.convertTo(alpha8, CV_8U, 255.0);
            cv::Mat blended8; blended.convertTo(blended8, CV_8U, 255.0);
            cv::imwrite(std::string(pTex.mb_str()), texBGR);
            cv::imwrite(std::string(pAlpha.mb_str()), alpha8);
            cv::imwrite(std::string(pBlend.mb_str()), blended8);
            // Print channel means over bright pixels of texture for diagnosis
            cv::Mat gray; cv::cvtColor(texBGR, gray, cv::COLOR_BGR2GRAY);
            cv::Mat mask; cv::threshold(gray, mask, 32, 255, cv::THRESH_BINARY);
            cv::Scalar m = cv::mean(texBGR, mask);
            std::cout << "[Develop Debug] Texture BGR mean over bright pixels: B=" << m[0] << " G=" << m[1] << " R=" << m[2] << std::endl;
        }

        // Save
        wxFileName inFn(currentImagePath_);
        wxString outName = inFn.GetName() + "_developed." + inFn.GetExt();
        wxString finalPath = wxFileName(outDir, outName).GetFullPath();
        bool ok = cv::imwrite(std::string(finalPath.mb_str()), result8BGR);
        if (ok) {
            preview_->SetStatus(wxString::Format("Developed: %s (mode %d, %.0f%%)", wxFileName(finalPath).GetFullName(), mode, opacity*100.0f), false);
            // Update preview to match what we saved
            preview_->UpdatePreviewDevelop(currentImagePath_, texPath, mode, opacity, controls_->getUseTextureLuminance(), controls_->getSwapRB());
        } else {
            preview_->SetStatus("Failed to save developed image", true);
        }
    });
}

void WxMainFrame::OnQuit(wxCommandEvent&)
{
    Close(true);
}
