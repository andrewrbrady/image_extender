#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QTimer>

#include "../widgets/BatchListWidget.hpp"
#include "../models/ImageSettings.hpp"

class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;

/**
 * @class ControlPanel
 * @brief Panel containing all control elements for image processing
 *
 * This panel provides the user interface for:
 * - Batch image list management
 * - Canvas dimension controls
 * - Processing parameters
 * - Output folder selection
 * - Processing controls
 */
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a ControlPanel
     * @param parent The parent widget
     */
    explicit ControlPanel(QWidget *parent = nullptr);

    /**
     * @brief Gets the current settings from UI controls
     * @return ImageSettings struct containing current values
     */
    ImageSettings getCurrentSettings() const;

    /**
     * @brief Loads settings into UI controls
     * @param settings ImageSettings to load
     */
    void loadSettings(const ImageSettings &settings);

    /**
     * @brief Gets the current scale factor from the UI
     * @return Scale factor (1, 2, or 4)
     */
    int getScaleFactor() const;

    /**
     * @brief Gets the output folder path
     * @return Path to the output folder
     */
    QString getOutputFolder() const;

    /**
     * @brief Gets the list of batch files
     * @return List of file paths
     */
    QStringList getBatchFiles() const;

signals:
    /**
     * @brief Signal emitted when settings are changed
     */
    void settingsChanged();

    /**
     * @brief Signal emitted when processing is requested
     */
    void processRequested();

    /**
     * @brief Signal emitted when a batch item is selected
     * @param filePath Path to the selected file
     */
    void batchItemSelected(const QString &filePath);

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
     * @brief Creates the processing section
     * @return The processing group box
     */
    QGroupBox *createProcessingSection();

    /**
     * @brief Creates the canvas dimensions section
     * @return The canvas dimensions group box
     */
    QGroupBox *createCanvasDimensionsSection();

    /**
     * @brief Creates the processing parameters section
     * @return The processing parameters group box
     */
    QGroupBox *createProcessingParamsSection();

    /**
     * @brief Creates the output folder section
     * @return The output folder group box
     */
    QGroupBox *createOutputSection();

    // UI Components
    BatchListWidget *imageList;
    QSpinBox *widthBox;
    QSpinBox *heightBox;
    QSpinBox *thresholdBox;
    QDoubleSpinBox *paddingBox;
    QSpinBox *finalWidthBox;
    QSpinBox *finalHeightBox;
    QSpinBox *blurRadiusBox;
    QComboBox *scaleBox;
    QLineEdit *outputFolderEdit;
    QPushButton *processButton;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QTimer *previewTimer;

    // Data
    QStringList batchFiles;
};
