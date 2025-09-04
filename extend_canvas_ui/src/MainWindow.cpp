#include "MainWindow.hpp"
#include <QSplitter>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include "panels/ControlPanel.hpp"
#include "panels/PreviewPanel.hpp"
#include "extend_canvas.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent), mainSplitter(nullptr), controlPanel(nullptr), previewPanel(nullptr)
{
    setupStyle();
    setupUI();
    setupConnections();
}

void MainWindow::setupUI()
{
    // Configure window
    setWindowTitle("Extend Canvas");
    // Start wider to avoid initial horizontal scrolling
    resize(2200, 1300);
    setMinimumSize(1600, 900);

    // Create main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create splitter
    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);

    // Create panels
    controlPanel = new ControlPanel();
    previewPanel = new PreviewPanel();

    // Add panels to splitter
    mainSplitter->addWidget(controlPanel);
    mainSplitter->addWidget(previewPanel);
    mainSplitter->setSizes({380, 1220}); // Give more space to preview
    // Ensure preview pane flexes to consume extra width, avoiding initial H-scroll
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(mainSplitter);
}

void MainWindow::setupConnections()
{
    // Connect control panel signals
    connect(controlPanel, &ControlPanel::settingsChanged,
            this, &MainWindow::onSettingsChanged);
    connect(controlPanel, &ControlPanel::processRequested,
            this, &MainWindow::processImages);
    connect(controlPanel, &ControlPanel::batchItemSelected,
            this, &MainWindow::onBatchItemSelected);
}

void MainWindow::setupStyle()
{
    setStyleSheet(R"(
        QWidget {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            font-size: 13px;
            background-color: #1a1d21;
            color: #e0e0e0;
        }
        QGroupBox {
            font-weight: 600;
            font-size: 13px;
            color: #e0e0e0;
            border: 1px solid #333942;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 8px;
            background-color: #262a2f;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px 0 8px;
            background-color: transparent;
        }
        QLabel {
            color: #e0e0e0;
        }
        QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit {
            border: 1px solid #333942;
            border-radius: 6px;
            padding: 6px 8px;
            background-color: #1f2227;
            color: #e0e0e0;
            selection-background-color: #ff1744;
            selection-color: #ffffff;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QLineEdit:focus {
            border: 2px solid #ff1744;
        }
        QListWidget {
            border: 1px solid #333942;
            border-radius: 6px;
            background-color: #1f2227;
            color: #e0e0e0;
            selection-background-color: #ff1744;
            selection-color: #ffffff;
        }
        QListWidget:focus {
            border: 2px solid #ff1744;
        }
        QListWidget::item {
            padding: 4px;
            border-bottom: 1px solid #2d3138;
        }
        QListWidget::item:selected {
            background-color: #ff1744;
            color: #ffffff;
        }
        QListWidget::item:hover {
            background-color: #24292e;
        }
        QProgressBar {
            border: 1px solid #333942;
            border-radius: 6px;
            background-color: #2d3138;
            text-align: center;
            color: #e0e0e0;
        }
        QProgressBar::chunk {
            background-color: #ff1744;
            border-radius: 5px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #e0e0e0;
            margin-right: 5px;
        }
        QPushButton {
            border: 1px solid #ff1744;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 500;
            font-size: 13px;
            background-color: transparent;
            color: #e0e0e0;
        }
        QPushButton:hover {
            background-color: #2d3138;
        }
        QPushButton:pressed {
            background-color: #3a3f46;
        }
        QScrollArea {
            border: none;
            background-color: #1a1d21;
        }
    )");
}

void MainWindow::onSettingsChanged()
{
    if (!currentImagePath.isEmpty())
    {
        // Store current settings for the image
        imageSettings[currentImagePath] = controlPanel->getCurrentSettings();

        // Update preview
        previewPanel->updatePreview(currentImagePath, imageSettings[currentImagePath]);
    }
}

void MainWindow::onBatchItemSelected(const QString &filePath)
{
    currentImagePath = filePath;

    // Load settings for this image (or use defaults)
    if (imageSettings.contains(filePath))
    {
        controlPanel->loadSettings(imageSettings[filePath]);
    }
    else
    {
        imageSettings[filePath] = controlPanel->getCurrentSettings();
    }

    // Update preview
    previewPanel->updatePreview(filePath, imageSettings[filePath]);
}

void MainWindow::processImages()
{
    QStringList batchFiles = controlPanel->getBatchFiles();
    if (batchFiles.isEmpty())
        return;

    QString outputFolder = controlPanel->getOutputFolder();
    if (outputFolder.isEmpty())
    {
        previewPanel->setStatus("No output folder selected", true);
        return;
    }

    // Create output directory
    QDir().mkpath(outputFolder);

    int scaleFactor = controlPanel->getScaleFactor();
    int processed = 0;
    int successful = 0;

    for (const QString &file : batchFiles)
    {
        previewPanel->setStatus(QString("Processing %1 (%2/%3)...")
                                    .arg(QFileInfo(file).fileName())
                                    .arg(processed + 1)
                                    .arg(batchFiles.size()));

        // Use stored settings for this image
        ImageSettings settings = imageSettings.value(file, controlPanel->getCurrentSettings());

        // Apply scale factor to dimensions
        const int rw = settings.width * scaleFactor;
        const int rh = settings.height * scaleFactor;
        const int finalW = settings.finalWidth > 0 ? settings.finalWidth * scaleFactor : -1;
        const int finalH = settings.finalHeight > 0 ? settings.finalHeight * scaleFactor : -1;

        bool success = extendCanvas(file.toStdString(),
                                    rw, rh,
                                    settings.whiteThreshold, settings.padding,
                                    finalW, finalH,
                                    settings.blurRadius);

        if (success)
        {
            // Move the generated file to output folder
            QFileInfo inputFile(file);
            QString tempOutputPath = inputFile.dir().filePath(
                inputFile.baseName() + "_extended." + inputFile.suffix());

            QString outputFileName = inputFile.baseName() + "_extended";
            if (scaleFactor > 1)
            {
                outputFileName += QString("_%1x").arg(scaleFactor);
            }
            outputFileName += "." + inputFile.suffix();

            QString finalPath = QDir(outputFolder).filePath(outputFileName);

            if (QFile::rename(tempOutputPath, finalPath))
            {
                successful++;
            }
        }

        processed++;
    }

    previewPanel->setStatus(QString("Processing complete: %1/%2 images processed successfully")
                                .arg(successful)
                                .arg(batchFiles.size()),
                            successful != batchFiles.size());
}
