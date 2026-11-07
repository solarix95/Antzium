#pragma once

#include <QFileDialog>
#include <QTimer>

namespace FileDialogUtils
{
inline QString getFileName(
    QWidget* parent,
    const QString& title,
    const QString& nameFilter,
    QFileDialog::AcceptMode acceptMode
    )
{
    QFileDialog dialog(parent);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setWindowTitle(title);
    dialog.setNameFilter(nameFilter);
    dialog.setAcceptMode(acceptMode);
    dialog.setFileMode(
        acceptMode == QFileDialog::AcceptSave
            ? QFileDialog::AnyFile
            : QFileDialog::ExistingFile
        );
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);

    QTimer::singleShot(0, &dialog, [&dialog]() {
        dialog.raise();
        dialog.activateWindow();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }

    return dialog.selectedFiles().value(0);
}
}
