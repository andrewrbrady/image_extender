#include "PreviewPanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <algorithm>
#include <opencv2/opencv.hpp>

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent),
      originalTitleLabel(nullptr),
      originalImageLabel(nullptr),
      resultTitleLabel(nullptr),
      resultImageLabel(nullptr),
      overlayLabel(nullptr),
      overlayEffect(nullptr)
{
    setupUI();
}

void PreviewPanel::setupUI()
{
    setObjectName("previewFrame");
    setStyleSheet(R"(
        QWidget#previewFrame {
            background-color: #1f2227;
            border: 1px solid #333942;
            border-radius: 8px;
        }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // Preview images container (no scroll area to avoid horizontal scrollbars)
    QWidget *container = new QWidget();
    // Let container ignore its horizontal size hint so it shrinks to viewport
    container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    QHBoxLayout *containerLayout = new QHBoxLayout(container);
    containerLayout->setSpacing(16);
    containerLayout->setContentsMargins(16, 16, 16, 16);

    // Create original and result containers
    auto [originalContainer, originalLabel] = createImageContainer("Original", "No image loaded");
    auto [resultContainer, resultLabel] = createImageContainer("Result", "No preview available");

    originalImageLabel = originalLabel;
    resultImageLabel = resultLabel;

    containerLayout->addWidget(originalContainer);
    containerLayout->addWidget(resultContainer);
    // Ensure both columns flex equally and can shrink to avoid initial H-scroll
    containerLayout->setStretch(0, 1);
    containerLayout->setStretch(1, 1);

    mainLayout->addWidget(container, 1);

    // Transient overlay that doesn't consume layout space
    overlayLabel = new QLabel(this);
    overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlayLabel->setAlignment(Qt::AlignCenter);
    overlayLabel->setText("");
    overlayLabel->setStyleSheet(R"(
        QLabel {
            background: transparent;
            border: none;
        }
    )");
    overlayEffect = new QGraphicsOpacityEffect(overlayLabel);
    overlayEffect->setOpacity(0.0);
    overlayLabel->setGraphicsEffect(overlayEffect);
    overlayLabel->hide();
}

std::pair<QWidget *, QLabel *> PreviewPanel::createImageContainer(
    const QString &title,
    const QString &placeholder)
{
    QWidget *widget = new QWidget();
    // Ensure each column expands but does not force a width (avoid H-scroll)
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(12);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(R"(
        QLabel { 
            font-weight: 600; 
            font-size: 14px;
            color: #333;
            padding: 8px;
            background: none;
            border: none;
        }
    )");

    // Container for image that uses full height
    QWidget *imageContainer = new QWidget();
    // Allow height to follow viewport; avoid any minimum width that forces H-scroll
    imageContainer->setMinimumWidth(0);
    imageContainer->setMinimumHeight(0);
    imageContainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    imageContainer->setStyleSheet(R"(
        QWidget { 
            border: 2px dashed #333942; 
            background-color: #1f2227;
            border-radius: 8px;
        }
    )");

    QVBoxLayout *imageLayout = new QVBoxLayout(imageContainer);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setAlignment(Qt::AlignCenter);

    QLabel *imageLabel = new QLabel(placeholder);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet(R"(
        QLabel { 
            border: none;
            background: transparent;
            color: #999;
        }
    )");
    imageLabel->setScaledContents(false);

    imageLayout->addWidget(imageLabel);

    layout->addWidget(titleLabel);
    layout->addWidget(imageContainer, 1);

    if (title == "Original")
    {
        originalTitleLabel = titleLabel;
    }
    else
    {
        resultTitleLabel = titleLabel;
    }

    return {widget, imageLabel};
}

void PreviewPanel::updateImageDisplay(QLabel *label, QLabel *titleLabel, const QPixmap &pixmap)
{
    if (!label || !titleLabel)
    {
        return;
    }

    if (!pixmap.isNull())
    {
        QWidget *container = label->parentWidget();
        if (!container)
        {
            return;
        }

        QSize containerSize = container->size();
        if (containerSize.isEmpty())
        {
            // Use a minimal fallback; real size will be applied on first layout/resize
            containerSize = QSize(1, 1);
        }

        const int targetW = std::max(1, containerSize.width() - 40);
        const int targetH = std::max(1, containerSize.height() - 40);
        QPixmap scaled = pixmap.scaled(
            targetW,
            targetH,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);

        label->setPixmap(scaled);

        QString baseTitle = titleLabel->text().split(" ").first();
        if (baseTitle.isEmpty())
        {
            baseTitle = "Image";
        }

        titleLabel->setText(QString("%1 (%2×%3)")
                                .arg(baseTitle)
                                .arg(pixmap.width())
                                .arg(pixmap.height()));
    }
}

void PreviewPanel::updatePreview(const QString &imagePath, const ImageSettings &settings)
{
    if (imagePath.isEmpty())
    {
        clearPreview();
        return;
    }

    // Clean up any previous preview file
    if (!lastResultPath.isEmpty() && QFile::exists(lastResultPath))
    {
        QFile::remove(lastResultPath);
    }
    lastResultPath.clear();

    currentImagePath = imagePath;

    // Load and display original image
    QPixmap originalPixmap(imagePath);
    if (!originalPixmap.isNull())
    {
        try
        {
            originalPixmapCache = originalPixmap;
            updateImageDisplay(originalImageLabel, originalTitleLabel, originalPixmapCache);

            // Clear previous result display
            resultImageLabel->clear();
            resultImageLabel->setText("Generating preview...");
            resultTitleLabel->setText("Result");

            // In-memory preview (no lossy file I/O). Mirrors backend logic with Lanczos scaling.
            cv::Mat img = cv::imread(imagePath.toStdString());
            if (img.empty()) { setStatus("Failed to load image", true); return; }

            auto centerSampleThreshold = [](const cv::Mat &im, int stripeH = 20, int stripeW = 40)
            {
                int cx = im.cols / 2;
                int w = std::min({stripeW, std::max(1, cx - 1), std::max(1, im.cols - cx - 1)});
                int h = std::min(stripeH, std::max(1, im.rows / 10));
                cv::Rect topR(cx - w, 0, 2 * w + 1, h);
                cv::Rect botR(cx - w, im.rows - h, 2 * w + 1, h);
                cv::Mat gt, gb; cv::cvtColor(im(topR), gt, cv::COLOR_BGR2GRAY); cv::cvtColor(im(botR), gb, cv::COLOR_BGR2GRAY);
                double mt = cv::mean(gt)[0], mb = cv::mean(gb)[0];
                int thr = static_cast<int>(std::min(mt, mb) - 5.0);
                return std::clamp(thr, 180, 250);
            };
            auto findForegroundBounds = [](const cv::Mat &im, int &top, int &bot, int whiteThr)
            {
                cv::Mat mask; cv::inRange(im, cv::Scalar(whiteThr, whiteThr, whiteThr), cv::Scalar(255, 255, 255), mask);
                cv::bitwise_not(mask, mask); cv::reduce(mask, mask, 1, cv::REDUCE_MAX, CV_8U);
                top = bot = -1; for (int r = 0; r < mask.rows; ++r) { if (mask.at<uchar>(r, 0)) { if (top == -1) top = r; bot = r; } }
                return top != -1;
            };
            auto makeStrip = [](const cv::Mat &src, int newH, int W)
            {
                if (newH <= 0) return cv::Mat();
                if (!src.empty()) { cv::Mat dst; cv::resize(src, dst, cv::Size(W, newH), 0, 0, cv::INTER_AREA); return dst; }
                return cv::Mat(newH, W, CV_8UC3, cv::Scalar(255, 255, 255));
            };
            // Horizontal bounds helper removed (horizontal extension disabled)
            auto applyFinalResize = [](const cv::Mat &canvas, int reqW, int reqH)
            {
                if (reqW <= 0 || reqH <= 0) return canvas.clone();
                double sx = static_cast<double>(reqW) / canvas.cols;
                double sy = static_cast<double>(reqH) / canvas.rows; double s = std::min(sx, sy);
                int nw = std::max(1, static_cast<int>(canvas.cols * s + 0.5));
                int nh = std::max(1, static_cast<int>(canvas.rows * s + 0.5));
                cv::Mat resized; cv::resize(canvas, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LANCZOS4);
                cv::Mat final(reqH, reqW, canvas.type(), cv::Scalar(255, 255, 255));
                int x = (reqW - nw) / 2, y = (reqH - nh) / 2; resized.copyTo(final(cv::Rect(x, y, nw, nh))); return final;
            };
            auto blendSeam = [](cv::Mat &img, int seamX, int overlap)
            {
                if (overlap <= 0) return;
                if (seamX - overlap < 0 || seamX + overlap > img.cols) return;
                cv::Mat left = img(cv::Rect(seamX - overlap, 0, overlap, img.rows)).clone();
                cv::Mat right = img(cv::Rect(seamX, 0, overlap, img.rows)).clone();
                for (int i = 0; i < overlap; ++i)
                {
                    double a = (i + 1.0) / (overlap + 1.0);
                    cv::Mat dst = img(cv::Rect(seamX - overlap + i, 0, 1, img.rows));
                    cv::addWeighted(right.col(i), a, left.col(i), 1.0 - a, 0.0, dst);
                }
            };

            int thr = (settings.whiteThreshold >= 0 && settings.whiteThreshold <= 255)
                          ? settings.whiteThreshold
                          : centerSampleThreshold(img);
            int fgTop, fgBot; if (!findForegroundBounds(img, fgTop, fgBot, thr)) { setStatus("Foreground not found", true); return; }
            int carH = fgBot - fgTop + 1;
            int pad = static_cast<int>(carH * settings.padding + 0.5);
            int cropTop = std::max(0, fgTop - pad);
            int cropBot = std::min(img.rows - 1, fgBot + pad);
            cv::Mat carReg = img.rowRange(cropTop, cropBot + 1);

            int desiredW = settings.width > 0 ? settings.width : img.cols;
            int desiredH = settings.height > 0 ? settings.height : img.rows;
            int W = img.cols;
            // Horizontal extension disabled; no left/right metrics

            cv::Mat result;
            if (desiredH <= carReg.rows)
            {
                int yOff = (carReg.rows - desiredH) / 2; result = carReg.rowRange(yOff, yOff + desiredH).clone();
                if (false && desiredW > result.cols)
                {
                    auto makeStripW = [](const cv::Mat &src, int H, int newW)
                    {
                        if (newW <= 0) return cv::Mat();
                        if (!src.empty()) { cv::Mat dst; cv::resize(src, dst, cv::Size(newW, H), 0, 0, cv::INTER_AREA); return dst; }
                        return cv::Mat(H, newW, CV_8UC3, cv::Scalar(255, 255, 255));
                    };
                    int extraW = desiredW - result.cols; int leftW = extraW/2; int rightW = extraW - leftW;
                    cv::Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : cv::Mat();
                    cv::Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : cv::Mat();
                    cv::Mat leftStrip = makeStripW(leftSrc, result.rows, leftW);
                    cv::Mat rightStrip = makeStripW(rightSrc, result.rows, rightW);
                    if (settings.blurRadius > 0)
                    { int k = std::max(1, settings.blurRadius*2+1); if(!leftStrip.empty()) cv::GaussianBlur(leftStrip, leftStrip, cv::Size(k,k), 0); if(!rightStrip.empty()) cv::GaussianBlur(rightStrip, rightStrip, cv::Size(k,k), 0); }
                    cv::Mat wide(desiredH, desiredW, img.type()); int x=0;
                    if (!leftStrip.empty()) { leftStrip.copyTo(wide(cv::Rect(x,0,leftStrip.cols,leftStrip.rows))); x+=leftStrip.cols; }
                    result.copyTo(wide(cv::Rect(x,0,result.cols,result.rows))); x+=result.cols;
                    if (!rightStrip.empty()) { rightStrip.copyTo(wide(cv::Rect(x,0,rightStrip.cols,rightStrip.rows))); }
                    // seam blend at joins
                    if (!leftStrip.empty()) { int seamX = leftStrip.cols; int ov = std::min({24, leftStrip.cols, wide.cols - seamX}); blendSeam(wide, seamX, ov); }
                    if (!rightStrip.empty()) { int seamX = wide.cols - rightStrip.cols; int ov = std::min({24, rightStrip.cols, seamX}); blendSeam(wide, seamX, ov); }
                    result = wide;
                }
                if (desiredW != result.cols)
                {
                    double scale = static_cast<double>(desiredW) / result.cols; int sh = static_cast<int>(result.rows * scale + 0.5);
                    cv::resize(result, result, cv::Size(desiredW, sh), 0, 0, cv::INTER_LANCZOS4);
                    if (sh != desiredH)
                    {
                        if (sh > desiredH) { int y = (sh - desiredH) / 2; result = result.rowRange(y, y + desiredH).clone(); }
                        else { cv::Mat ext(desiredH, desiredW, result.type(), cv::Scalar(255, 255, 255)); int y = (desiredH - sh) / 2; result.copyTo(ext.rowRange(y, y + sh)); result = ext; }
                    }
                }
            }
            else
            {
                int extra = desiredH - carReg.rows; int topH = extra / 2; int botH = extra - topH; int targetW = desiredW;
                cv::Mat scaledCarReg = carReg; cv::Mat scaledTopSrc, scaledBotSrc;
                if (desiredW != W)
                {
                    double sc = static_cast<double>(desiredW) / W; int sh = static_cast<int>(carReg.rows * sc + 0.5);
                    cv::resize(carReg, scaledCarReg, cv::Size(desiredW, sh), 0, 0, cv::INTER_LANCZOS4);
                    cv::Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : cv::Mat();
                    cv::Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : cv::Mat();
                    if (!topSrc.empty()) { int sth = static_cast<int>(topSrc.rows * sc + 0.5); cv::resize(topSrc, scaledTopSrc, cv::Size(desiredW, sth), 0, 0, cv::INTER_LANCZOS4); }
                    if (!botSrc.empty()) { int sbh = static_cast<int>(botSrc.rows * sc + 0.5); cv::resize(botSrc, scaledBotSrc, cv::Size(desiredW, sbh), 0, 0, cv::INTER_LANCZOS4); }
                    extra = desiredH - scaledCarReg.rows; topH = extra / 2; botH = extra - topH;
                }
                else
                {
                    scaledTopSrc = cropTop > 0 ? img.rowRange(0, cropTop) : cv::Mat();
                    scaledBotSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : cv::Mat();
                }
                cv::Mat topStrip = makeStrip(scaledTopSrc, topH, targetW);
                cv::Mat botStrip = makeStrip(scaledBotSrc, botH, targetW);
                if (settings.blurRadius > 0)
                { int k = std::max(1, settings.blurRadius * 2 + 1); if (!topStrip.empty()) cv::GaussianBlur(topStrip, topStrip, cv::Size(k, k), 0); if (!botStrip.empty()) cv::GaussianBlur(botStrip, botStrip, cv::Size(k, k), 0); }
                result.create(desiredH, targetW, img.type());
                int y = 0; if (!topStrip.empty()) { topStrip.copyTo(result.rowRange(y, y + topStrip.rows)); y += topStrip.rows; }
                scaledCarReg.copyTo(result.rowRange(y, y + scaledCarReg.rows)); y += scaledCarReg.rows; if (!botStrip.empty()) botStrip.copyTo(result.rowRange(y, y + botStrip.rows));

                // Extend horizontally if needed and enabled
                if (false && desiredW > result.cols)
                {
                    auto makeStripW = [](const cv::Mat &src, int H, int newW)
                    {
                        if (newW <= 0) return cv::Mat();
                        if (!src.empty()) { cv::Mat dst; cv::resize(src, dst, cv::Size(newW, H), 0, 0, cv::INTER_AREA); return dst; }
                        return cv::Mat(H, newW, CV_8UC3, cv::Scalar(255, 255, 255));
                    };
                    int extraW = desiredW - result.cols; int leftW = extraW/2; int rightW = extraW - leftW;
                    cv::Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : cv::Mat();
                    cv::Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : cv::Mat();
                    cv::Mat leftStrip = makeStripW(leftSrc, result.rows, leftW);
                    cv::Mat rightStrip = makeStripW(rightSrc, result.rows, rightW);
                    if (settings.blurRadius > 0)
                    { int k = std::max(1, settings.blurRadius*2+1); if(!leftStrip.empty()) cv::GaussianBlur(leftStrip, leftStrip, cv::Size(k,k), 0); if(!rightStrip.empty()) cv::GaussianBlur(rightStrip, rightStrip, cv::Size(k,k), 0); }
                    cv::Mat wide(desiredH, desiredW, img.type()); int x=0;
                    if (!leftStrip.empty()) { leftStrip.copyTo(wide(cv::Rect(x,0,leftStrip.cols,leftStrip.rows))); x+=leftStrip.cols; }
                    result.copyTo(wide(cv::Rect(x,0,result.cols,result.rows))); x+=result.cols;
                    if (!rightStrip.empty()) { rightStrip.copyTo(wide(cv::Rect(x,0,rightStrip.cols,rightStrip.rows))); }
                    result = wide; // feature disabled
                }
            }
            result = applyFinalResize(result, settings.finalWidth, settings.finalHeight);

            // Convert to QPixmap (deep copy)
            cv::Mat rgb; cv::cvtColor(result, rgb, cv::COLOR_BGR2RGB);
            QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
            resultPixmapCache = QPixmap::fromImage(qimg.copy());
            updateImageDisplay(resultImageLabel, resultTitleLabel, resultPixmapCache);
            showSuccessTick();
        }
        catch (const std::exception &e)
        {
            setStatus(QString("Error: %1").arg(e.what()), true);
        }
        catch (...)
        {
            setStatus("Unexpected error during preview generation", true);
        }
    }
    else
    {
        setStatus("Failed to load image", true);
    }
}

void PreviewPanel::clearPreview()
{
    lastResultPath.clear();
    currentImagePath.clear();

    originalImageLabel->clear();
    originalImageLabel->setText("No image loaded");
    originalTitleLabel->setText("Original");
    originalPixmapCache = QPixmap();

    resultImageLabel->clear();
    resultImageLabel->setText("No preview available");
    resultTitleLabel->setText("Result");
    resultPixmapCache = QPixmap();

    // No persistent status
}

void PreviewPanel::setStatus(const QString &message, bool isError)
{
    // Show transient red error overlay; ignore non-error to keep UI clean
    if (isError)
    {
        showOverlayMessage(message, QColor("#ff3b30"), 14, 1600);
    }
}

void PreviewPanel::showSuccessTick()
{
    showOverlayMessage(QString::fromUtf8("✓"), QColor("#34C759"), 64, 1000);
}

void PreviewPanel::showOverlayMessage(const QString &text, const QColor &color, int fontPointSize, int durationMs)
{
    if (!overlayLabel || !overlayEffect)
        return;

    overlayLabel->setGeometry(rect());
    QFont f = overlayLabel->font();
    f.setPointSize(fontPointSize);
    f.setWeight(QFont::DemiBold);
    overlayLabel->setFont(f);
    overlayLabel->setText(text);
    overlayLabel->setStyleSheet(QString(R"(
        QLabel {
            background: transparent;
            color: %1;
        }
    )").arg(color.name()));

    overlayLabel->show();

    // Animate opacity: fade in, hold briefly, fade out
    overlayEffect->setOpacity(0.0);

    // Fade in
    auto *fadeIn = new QPropertyAnimation(overlayEffect, "opacity", overlayLabel);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);

    // Hold, then fade out
    QObject::connect(fadeIn, &QPropertyAnimation::finished, overlayLabel, [this, durationMs]() {
        QTimer::singleShot(durationMs, this, [this]() {
            auto *fadeOut = new QPropertyAnimation(overlayEffect, "opacity", overlayLabel);
            fadeOut->setDuration(260);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
            QObject::connect(fadeOut, &QPropertyAnimation::finished, overlayLabel, [this]() {
                overlayLabel->hide();
            });
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        });
    });

    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void PreviewPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (overlayLabel)
    {
        overlayLabel->setGeometry(rect());
    }
    // Rescale images to fit new viewport height
    if (!originalPixmapCache.isNull() && originalImageLabel && originalTitleLabel)
    {
        updateImageDisplay(originalImageLabel, originalTitleLabel, originalPixmapCache);
    }
    if (!resultPixmapCache.isNull() && resultImageLabel && resultTitleLabel)
    {
        updateImageDisplay(resultImageLabel, resultTitleLabel, resultPixmapCache);
    }
}
