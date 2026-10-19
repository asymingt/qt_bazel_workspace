#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>

// F-INF-7: Public qt_cc_library must NOT define QT_BUILDING_QT.
#ifdef QT_BUILDING_QT
#error "QT_BUILDING_QT must not be defined in user qt_cc_library targets!"
#endif

// In Qt 6.9+, Q_DECLARE_LOGGING_CATEGORY in a header + Q_LOGGING_CATEGORY in a .cpp
// fails to link across translation units if QT_BUILDING_QT is defined without export macros.
Q_DECLARE_LOGGING_CATEGORY(testConsumerLog)

class ConsumerObject : public QObject {
    Q_OBJECT
public:
    explicit ConsumerObject(QObject *parent = nullptr);
    QString greeting() const;
};
