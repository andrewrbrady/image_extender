#pragma once

#include <QWidget>
#include <QMap>
#include "models/ImageSettings.hpp"

class QSplitter;
class ControlPanel;
class PreviewPanel;

/**
 * @class MainWindow
 * @brief Main application window for the Extend Canvas UI
 *
 * This class serves as the main window of the application, coordinating
 * between the control panel and preview panel. It handles:
 * - Overall window layout and styling
 * - Image processing coordination
 * - Settings management
 * - Batch processing
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the main window
     * @param parent The parent widget
     */
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    /**
     * @brief Handles settings changes from the control panel
     */
    void onSettingsChanged();

    /**
     * @brief Handles batch item selection
     * @param filePath Path to the selected image
     */
    void onBatchItemSelected(const QString &filePath);

    /**
     * @brief Processes all images in the batch
     */
    void processImages();

private:
    /**
     * @brief Sets up the UI components
     */
    void setupUI();

    /**
     * @brief Sets up signal/slot connections
     */
    void setupConnections();

    /**
     * @brief Applies the application-wide stylesheet
     */
    void setupStyle();

    // UI Components
    QSplitter *mainSplitter;
    ControlPanel *controlPanel;
    PreviewPanel *previewPanel;

    // Data
    QMap<QString, ImageSettings> imageSettings; ///< Per-image settings storage
    QString currentImagePath;                   ///< Currently selected image
};