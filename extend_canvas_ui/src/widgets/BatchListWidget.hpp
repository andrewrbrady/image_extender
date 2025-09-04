#pragma once

#include <QListWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPainter>

/**
 * @class BatchListWidget
 * @brief Custom QListWidget that handles drag and drop for batch image processing
 *
 * This widget extends QListWidget to provide drag and drop functionality
 * specifically for image files. It includes visual feedback during drag
 * operations and maintains a clean, modern appearance.
 */
class BatchListWidget : public QListWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a BatchListWidget
     * @param parent The parent widget
     */
    explicit BatchListWidget(QWidget *parent = nullptr);

signals:
    /**
     * @brief Signal emitted when images are dropped
     * @param filePaths List of dropped image file paths
     */
    void imagesDropped(const QStringList &filePaths);

protected:
    /**
     * @brief Handles drag enter events
     * @param event The drag enter event
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief Handles drag move events
     * @param event The drag move event
     */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /**
     * @brief Handles drag leave events
     * @param event The drag leave event
     */
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    /**
     * @brief Handles drop events
     * @param event The drop event
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * @brief Custom paint event handler
     * @param event The paint event
     */
    void paintEvent(QPaintEvent *event) override;

private:
    /**
     * @brief Updates the widget's empty state appearance
     */
    void updateEmptyState();
};