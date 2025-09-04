#include "ControlPanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent), imageList(nullptr), widthBox(nullptr), heightBox(nullptr), thresholdBox(nullptr), paddingBox(nullptr), finalWidthBox(nullptr), finalHeightBox(nullptr), blurRadiusBox(nullptr), scaleBox(nullptr), outputFolderEdit(nullptr), processButton(nullptr), progressBar(nullptr), statusLabel(nullptr), previewTimer(new QTimer(this))
{
    // Configure preview timer first
    previewTimer->setSingleShot(true);
    previewTimer->setInterval(500); // 500ms delay

    // Set up UI components
    setupUI();

    // Set up connections AFTER UI is created
    setupConnections();
}

ImageSettings ControlPanel::getCurrentSettings() const
{
    // Return default settings if UI is not initialized
    if (!widthBox || !heightBox || !thresholdBox || !paddingBox)
    {
        return ImageSettings();
    }

    ImageSettings s(
        widthBox->value(),
        heightBox->value(),
        thresholdBox->value(),
        paddingBox->value(),
        -1, // finalWidth - not used
        -1  // finalHeight - not used
    );
    if (blurRadiusBox)
        s.blurRadius = blurRadiusBox->value();
    return s;
}

void ControlPanel::loadSettings(const ImageSettings &settings)
{
    // Don't try to load settings if UI is not initialized
    if (!widthBox || !heightBox || !thresholdBox || !paddingBox)
    {
        return;
    }

    widthBox->setValue(settings.width);
    heightBox->setValue(settings.height);
    thresholdBox->setValue(settings.whiteThreshold);
    paddingBox->setValue(settings.padding);
    // finalWidth and finalHeight are not used in the UI
    if (blurRadiusBox)
        blurRadiusBox->setValue(settings.blurRadius);
}

int ControlPanel::getScaleFactor() const
{
    if (!scaleBox)
    {
        return 1;
    }

    QString scaleText = scaleBox->currentText();
    if (scaleText == "2×")
        return 2;
    if (scaleText == "4×")
        return 4;
    return 1;
}

QString ControlPanel::getOutputFolder() const
{
    if (!outputFolderEdit)
    {
        return QString();
    }
    return outputFolderEdit->text();
}

QStringList ControlPanel::getBatchFiles() const
{
    return batchFiles;
}

void ControlPanel::setupUI()
{
    setMaximumWidth(380);
    setMinimumWidth(360);
    setStyleSheet("QWidget { background-color: #1a1d21; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Add sections
    mainLayout->addWidget(createProcessingSection());
    mainLayout->addWidget(createCanvasDimensionsSection());
    mainLayout->addWidget(createProcessingParamsSection());
    mainLayout->addWidget(createOutputSection());

    // Add status label
    statusLabel = new QLabel("Ready - Add images to begin");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    statusLabel->setMinimumHeight(40);
    statusLabel->setStyleSheet(R"(
        QLabel { 
            padding: 12px; 
            background-color: white; 
            border: 1px solid #e0e0e0; 
            border-radius: 6px;
            color: #666;
        }
    )");
    mainLayout->addWidget(statusLabel);

    mainLayout->addStretch();
}

void ControlPanel::setupConnections()
{
    // Ensure all widgets are initialized before setting up connections
    if (!widthBox || !heightBox || !thresholdBox || !paddingBox ||
        !imageList || !processButton || !previewTimer)
    {
        return;
    }

    // Connect preview timer
    connect(previewTimer, &QTimer::timeout, this, &ControlPanel::settingsChanged);

    // Connect all spinboxes to trigger preview
    auto connectSpinBox = [this](QSpinBox *box)
    {
        if (box)
        {
            connect(box, QOverload<int>::of(&QSpinBox::valueChanged),
                    previewTimer, QOverload<>::of(&QTimer::start));
        }
    };

    connectSpinBox(widthBox);
    connectSpinBox(heightBox);
    connectSpinBox(thresholdBox);
    connectSpinBox(blurRadiusBox);

    if (paddingBox)
    {
        connect(paddingBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                previewTimer, QOverload<>::of(&QTimer::start));
    }

    

    // Connect batch list signals
    if (imageList)
    {
        // Test connection - this should work if MOC is working properly
        connect(imageList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {});

        connect(imageList, &BatchListWidget::imagesDropped,
                [this](const QStringList &files)
                {
                    bool wasEmpty = batchFiles.isEmpty();

                    for (const QString &file : files)
                    {
                        if (!batchFiles.contains(file))
                        {
                            batchFiles.append(file);
                            imageList->addItem(QFileInfo(file).fileName());
                        }
                    }

                    if (processButton)
                    {
                        processButton->setEnabled(!batchFiles.isEmpty());
                    }

                    if (!batchFiles.isEmpty() && outputFolderEdit && outputFolderEdit->text().isEmpty())
                    {
                        QFileInfo firstFile(batchFiles.first());
                        QString defaultOutputFolder = firstFile.dir().filePath("extended_images");
                        outputFolderEdit->setText(defaultOutputFolder);
                    }

                    // If this was the first batch of files, select the first one
                    if (wasEmpty && !batchFiles.isEmpty())
                    {
                        imageList->setCurrentRow(0);
                    }
                });

        connect(imageList, &QListWidget::currentItemChanged,
                [this](QListWidgetItem *current, QListWidgetItem *)
                {
                    if (current)
                    {
                        int row = imageList->row(current);
                        if (row >= 0 && row < batchFiles.size())
                        {
                            emit batchItemSelected(batchFiles[row]);
                        }
                    }
                });
    }
    else
    {
        // qDebug() << "imageList is null, cannot set up connections";
    }

    // Connect process button
    if (processButton)
    {
        connect(processButton, &QPushButton::clicked,
                this, &ControlPanel::processRequested);
    }
}

QGroupBox *ControlPanel::createProcessingSection()
{
    QGroupBox *group = new QGroupBox("Processing");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 20, 12, 12);

    // Create batch list
    imageList = new BatchListWidget();
    layout->addWidget(imageList);

    // Create buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    QPushButton *addButton = new QPushButton("Add Images...");
    QPushButton *clearButton = new QPushButton("Clear");

    QString buttonStyle = R"(
        QPushButton {
            background-color: #2d3138;
            color: #e0e0e0;
            border: 1px solid #333942;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #3a3f46;
        }
        QPushButton:pressed {
            background-color: #4a4f57;
        }
    )";

    addButton->setStyleSheet(buttonStyle);
    clearButton->setStyleSheet(buttonStyle);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // Create progress bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    layout->addWidget(progressBar);

    // Connect buttons
    connect(addButton, &QPushButton::clicked, [this]
            {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Images for Processing",
            "", "Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.tif)");
        
        for (const QString& file : files) {
            if (!batchFiles.contains(file)) {
                batchFiles.append(file);
                imageList->addItem(QFileInfo(file).fileName());
            }
        }
        
        processButton->setEnabled(!batchFiles.isEmpty());
        
        if (!batchFiles.isEmpty() && outputFolderEdit->text().isEmpty()) {
            QFileInfo firstFile(batchFiles.first());
            QString defaultOutputFolder = firstFile.dir().filePath("extended_images");
            outputFolderEdit->setText(defaultOutputFolder);
        } });

    connect(clearButton, &QPushButton::clicked, [this]
            {
        batchFiles.clear();
        imageList->clear();
        processButton->setEnabled(false); });

    return group;
}

QGroupBox *ControlPanel::createCanvasDimensionsSection()
{
    QGroupBox *group = new QGroupBox("Canvas Dimensions");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 20, 12, 12);

    // Create presets section
    QLabel *presetsLabel = new QLabel("Quick Presets:");
    presetsLabel->setStyleSheet("QLabel { font-weight: 600; margin-bottom: 4px; }");
    layout->addWidget(presetsLabel);

    QHBoxLayout *presetsLayout = new QHBoxLayout();
    presetsLayout->setSpacing(8);

    QString presetStyle = R"(
        QPushButton {
            background-color: #2d3138;
            color: #e0e0e0;
            border: 1px solid #333942;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #3a3f46;
        }
        QPushButton:pressed {
            background-color: #4a4f57;
        }
    )";

    auto createPresetButton = [&](const QString &text, int w, int h)
    {
        QPushButton *btn = new QPushButton(text);
        btn->setStyleSheet(presetStyle);
        connect(btn, &QPushButton::clicked, [=]
                {
            widthBox->setValue(w);
            heightBox->setValue(h); });
        return btn;
    };

    presetsLayout->addWidget(createPresetButton("1080×1350", 1080, 1350));
    presetsLayout->addWidget(createPresetButton("1080×1920", 1080, 1920));
    presetsLayout->addWidget(createPresetButton("1080×1080", 1080, 1080));

    layout->addLayout(presetsLayout);

    // Create dimension inputs
    QHBoxLayout *dimensionsLayout = new QHBoxLayout();
    dimensionsLayout->setSpacing(12);

    auto createSpinBox = [](int value, const QString &suffix = " px")
    {
        QSpinBox *box = new QSpinBox();
        box->setRange(0, 20000);
        box->setValue(value);
        box->setSuffix(suffix);
        box->setMinimumHeight(32);
        box->setMinimumWidth(100);
        return box;
    };

    QLabel *widthLabel = new QLabel("Width:");
    widthLabel->setStyleSheet("QLabel { font-weight: 500; }");
    widthBox = createSpinBox(1080);
    widthBox->setSpecialValueText("Auto");

    QLabel *heightLabel = new QLabel("Height:");
    heightLabel->setStyleSheet("QLabel { font-weight: 500; }");
    heightBox = createSpinBox(1920);

    dimensionsLayout->addWidget(widthLabel);
    dimensionsLayout->addWidget(widthBox);
    dimensionsLayout->addWidget(heightLabel);
    dimensionsLayout->addWidget(heightBox);
    dimensionsLayout->addStretch();

    layout->addLayout(dimensionsLayout);

    return group;
}

QGroupBox *ControlPanel::createProcessingParamsSection()
{
    QGroupBox *group = new QGroupBox("Processing Parameters");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 20, 12, 12);

    QHBoxLayout *paramsLayout = new QHBoxLayout();
    paramsLayout->setSpacing(12);

    // Create threshold control
    QLabel *threshLabel = new QLabel("White Threshold:");
    threshLabel->setStyleSheet("QLabel { font-weight: 500; }");
    thresholdBox = new QSpinBox();
    thresholdBox->setRange(-1, 255);
    thresholdBox->setValue(20);
    thresholdBox->setSpecialValueText("Auto");
    thresholdBox->setMinimumHeight(32);
    thresholdBox->setMinimumWidth(80);

    // Create padding control
    QLabel *padLabel = new QLabel("Padding %:");
    padLabel->setStyleSheet("QLabel { font-weight: 500; }");
    paddingBox = new QDoubleSpinBox();
    paddingBox->setRange(0.0, 1.0);
    paddingBox->setSingleStep(0.01);
    paddingBox->setDecimals(3);
    paddingBox->setValue(0.05);
    paddingBox->setSuffix("");
    paddingBox->setMinimumHeight(32);
    paddingBox->setMinimumWidth(80);

    paramsLayout->addWidget(threshLabel);
    paramsLayout->addWidget(thresholdBox);
    paramsLayout->addWidget(padLabel);
    paramsLayout->addWidget(paddingBox);
    
    // Create blur radius control (0 disables)
    QLabel *blurLabel = new QLabel("Blur radius:");
    blurLabel->setStyleSheet("QLabel { font-weight: 500; }");
    blurRadiusBox = new QSpinBox();
    blurRadiusBox->setRange(0, 50);
    blurRadiusBox->setValue(0);
    blurRadiusBox->setSuffix(" px");
    blurRadiusBox->setMinimumHeight(32);
    blurRadiusBox->setMinimumWidth(80);

    paramsLayout->addWidget(blurLabel);
    paramsLayout->addWidget(blurRadiusBox);
    
    
    paramsLayout->addStretch();

    layout->addLayout(paramsLayout);

    // Create scale selection
    QHBoxLayout *scaleLayout = new QHBoxLayout();
    scaleLayout->setSpacing(8);

    scaleBox = new QComboBox();
    scaleBox->addItem("1×");
    scaleBox->addItem("2×");
    scaleBox->addItem("4×");
    scaleBox->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #333942;
            border-radius: 6px;
            background-color: #1f2227;
            color: #e0e0e0;
            min-width: 80px;
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
    )");

    processButton = new QPushButton("Process Images");
    processButton->setMinimumHeight(44);
    processButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ff1744;
            color: #ffffff;
            border: none;
            font-weight: 600;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #e6003a;
        }
        QPushButton:pressed {
            background-color: #cc0034;
        }
        QPushButton:disabled {
            background-color: #555555;
            color: #888888;
        }
    )");
    processButton->setEnabled(false);

    scaleLayout->addWidget(scaleBox);
    scaleLayout->addWidget(processButton, 1);

    layout->addLayout(scaleLayout);

    return group;
}

QGroupBox *ControlPanel::createOutputSection()
{
    QGroupBox *group = new QGroupBox("Output Folder");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 20, 12, 12);

    outputFolderEdit = new QLineEdit();
    outputFolderEdit->setPlaceholderText("Select an image to set output folder");
    outputFolderEdit->setMinimumHeight(32);

    QPushButton *browseButton = new QPushButton("Browse...");
    browseButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2d3138;
            color: #e0e0e0;
            border: 1px solid #333942;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #3a3f46;
        }
        QPushButton:pressed {
            background-color: #4a4f57;
        }
    )");

    connect(browseButton, &QPushButton::clicked, [this]
            {
        QString folder = QFileDialog::getExistingDirectory(this,
            "Select Output Folder", outputFolderEdit->text());
        if (!folder.isEmpty()) {
            outputFolderEdit->setText(folder);
        } });

    layout->addWidget(outputFolderEdit);
    layout->addWidget(browseButton);

    return group;
}
