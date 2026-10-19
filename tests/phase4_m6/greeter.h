#pragma once

#include <QtCore/QString>
#include <QtCore/qglobal.h>

#if defined(M6_GREETER_BUILD)
#define M6_GREETER_EXPORT Q_DECL_EXPORT
#else
#define M6_GREETER_EXPORT Q_DECL_IMPORT
#endif

M6_GREETER_EXPORT QString m6Greeting();
