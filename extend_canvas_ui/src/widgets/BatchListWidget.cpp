#include "BatchListWidget.hpp"
#include <QFileInfo>

BatchListWidget::BatchListWidget(QWidget *parent) : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDefaultDropAction(Qt::CopyAction);
    setMinimumHeight(250);
    updateEmptyState();

    // Connect to item changes to update empty state
    connect(this->model(), &QAbstractItemModel::rowsInserted, this, &BatchListWidget::updateEmptyState);
    connect(this->model(), &QAbstractItemModel::rowsRemoved, this, &BatchListWidget::updateEmptyState);
}

void BatchListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        for (const QUrl &url : mimeData->urls())
        {
            if (url.isLocalFile())
            {
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                QString extension = fileInfo.suffix().toLower();
                if (extension == "png" || extension == "jpg" || extension == "jpeg" ||
                    extension == "bmp" || extension == "tiff" || extension == "tif")
                {
                    event->acceptProposedAction();
                    setStyleSheet(R"(
                        QListWidget {
                            border: 2px solid #ff1744;
                            border-radius: 8px;
                            background-color: #24292e;
                            color: #e0e0e0;
                        }
                    )");
                    return;
                }
            }
        }
    }
    event->ignore();
}

void BatchListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void BatchListWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    updateEmptyState();
    QListWidget::dragLeaveEvent(event);
}

void BatchListWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QStringList imageFiles;
        for (const QUrl &url : mimeData->urls())
        {
            if (url.isLocalFile())
            {
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                QString extension = fileInfo.suffix().toLower();
                if (extension == "png" || extension == "jpg" || extension == "jpeg" ||
                    extension == "bmp" || extension == "tiff" || extension == "tif")
                {
                    imageFiles.append(filePath);
                }
            }
        }

        if (!imageFiles.isEmpty())
        {
            event->acceptProposedAction();
            emit imagesDropped(imageFiles);
            updateEmptyState();
        }
    }
}

void BatchListWidget::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);

    if (count() == 0)
    {
        // Draw the placeholder text
        QPainter painter(viewport());
        painter.save();

        QFont font = painter.font();
        font.setPointSize(13);
        painter.setFont(font);

        QPen pen(QColor("#666666"));
        painter.setPen(pen);

        QRect textRect = viewport()->rect();
        painter.drawText(textRect, Qt::AlignCenter, "Drag & drop images here\nor click Add Images...");

        painter.restore();
    }
}

void BatchListWidget::updateEmptyState()
{
    if (count() == 0)
    {
        setStyleSheet(R"(
            QListWidget {
                border: 2px dashed #333942;
                border-radius: 8px;
                background-color: #1f2227;
                color: #888;
            }
        )");
    }
    else
    {
        setStyleSheet(R"(
            QListWidget {
                border: 1px solid #333942;
                border-radius: 8px;
                background-color: #1f2227;
                color: #e0e0e0;
            }
            QListWidget::item {
                padding: 8px;
                border-bottom: 1px solid #2d3138;
            }
            QListWidget::item:selected {
                background-color: #ff1744;
                color: #ffffff;
            }
            QListWidget::item:hover {
                background-color: #24292e;
            }
        )");
    }
}