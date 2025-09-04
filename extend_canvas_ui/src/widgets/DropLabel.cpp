#include "DropLabel.hpp"

const QStringList DropLabel::supportedExtensions = {"png", "jpg", "jpeg", "bmp", "tiff", "tif"};

DropLabel::DropLabel(QWidget *parent) : QLabel(parent)
{
    setAcceptDrops(true);
    setAlignment(Qt::AlignCenter);
    setWordWrap(true);
    setMinimumHeight(60);
    setStyleSheet(R"(
        QLabel { 
            padding: 16px; 
            background-color: white; 
            border: 2px dashed #c0c0c0; 
            border-radius: 8px;
            color: #666;
        }
    )");
    setText("Drag & drop an image here\nor click Choose Image...");
}

void DropLabel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QString fileName = urls.first().toLocalFile();
            QString extension = QFileInfo(fileName).suffix().toLower();

            if (supportedExtensions.contains(extension))
            {
                event->acceptProposedAction();
                setStyleSheet(R"(
                    QLabel { 
                        padding: 16px; 
                        background-color: #e8f5ff; 
                        border: 2px solid #007AFF; 
                        border-radius: 8px;
                        color: #333;
                    }
                )");
                return;
            }
        }
    }
    event->ignore();
}

void DropLabel::dragLeaveEvent(QDragLeaveEvent *event)
{
    setStyleSheet(R"(
        QLabel { 
            padding: 16px; 
            background-color: white; 
            border: 2px dashed #c0c0c0; 
            border-radius: 8px;
            color: #666;
        }
    )");
    QLabel::dragLeaveEvent(event);
}

void DropLabel::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QString fileName = urls.first().toLocalFile();
            QString extension = QFileInfo(fileName).suffix().toLower();

            if (supportedExtensions.contains(extension))
            {
                emit imageDropped(fileName);
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}