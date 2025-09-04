#pragma once

#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

/**
 * @class DropLabel
 * @brief Custom QLabel that accepts drag and drop for single image processing
 *
 * This widget extends QLabel to provide drag and drop functionality for
 * single image file selection. It includes visual feedback during drag
 * operations and maintains a modern, user-friendly appearance.
 */
class DropLabel : public QLabel
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a DropLabel
     * @param parent The parent widget
     */
    explicit DropLabel(QWidget *parent = nullptr);

signals:
    /**
     * @brief Signal emitted when an image is dropped
     * @param filePath Path to the dropped image file
     */
    void imageDropped(const QString &filePath);

protected:
    /**
     * @brief Handles drag enter events
     * @param event The drag enter event
     *
     * Validates incoming drag data for supported image file types
     * and provides visual feedback when a valid file is dragged over.
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief Handles drag leave events
     * @param event The drag leave event
     *
     * Restores the label's appearance when drag operation leaves.
     */
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    /**
     * @brief Handles drop events
     * @param event The drop event
     *
     * Processes a dropped image file and emits the imageDropped signal
     * if the file is a supported image format.
     */
    void dropEvent(QDropEvent *event) override;

private:
    /**
     * @brief List of supported image file extensions
     */
    static const QStringList supportedExtensions;
};