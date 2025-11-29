#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QString>
#include <QPixmap>

class OcrManager
{
public:
    static QString recognize(const QPixmap &pixmap);
    static QString getBackendType();
};

#endif // OCRMANAGER_H
