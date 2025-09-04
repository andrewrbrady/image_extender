#pragma once

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include "../models/ImageSettings.hpp"
#include <QColor>
class QGraphicsOpacityEffect;
class QPropertyAnimation;

class QVBoxLayout;
class QScrollArea;

/**
 * @class PreviewPanel
 * @brief Panel displaying original and processed image previews
 *
 * This panel provides a side-by-side view of:
 * - The original input image
 * - The processed result with current settings
 */
class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a PreviewPanel
     * @param parent The parent widget
     */
    explicit PreviewPanel(QWidget *parent = nullptr);

public slots:
    /**
     * @brief Updates the preview with a new image and settings
     * @param imagePath Path to the image file
     * @param settings Current processing settings
     */
    void updatePreview(const QString &imagePath, const ImageSettings &settings);

    /**
     * @brief Clears the preview display
     */
    void clearPreview();

    /**
     * @brief Sets the processing status message
     * @param message Status message to display
     * @param isError Whether the message indicates an error
     */
    void setStatus(const QString &message, bool isError = false);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /**
     * @brief Sets up the UI components
     */
    void setupUI();

    /**
     * @brief Creates an image container widget
     * @param title Title for the container
     * @param placeholder Placeholder text when no image
     * @return Pair of container widget and image label
     */
    std::pair<QWidget *, QLabel *> createImageContainer(
        const QString &title,
        const QString &placeholder);

    /**
     * @brief Updates an image display with a scaled pixmap
     * @param label Label to update
     * @param titleLabel Title label to update
     * @param pixmap Pixmap to display
     */
    void updateImageDisplay(QLabel *label, QLabel *titleLabel, const QPixmap &pixmap);

    // Overlay helpers
    void showSuccessTick();
    void showOverlayMessage(const QString &text, const QColor &color, int fontPointSize = 16, int durationMs = 1200);

    // UI Components
    QLabel *originalTitleLabel;
    QLabel *originalImageLabel;
    QLabel *resultTitleLabel;
    QLabel *resultImageLabel;
    // Transient overlay (centered)
    QLabel *overlayLabel;
    QGraphicsOpacityEffect *overlayEffect;

    // Current state
    QString currentImagePath;
    QString lastResultPath;

    // Cached pixmaps for responsive resizing
    QPixmap originalPixmapCache;
    QPixmap resultPixmapCache;
};
