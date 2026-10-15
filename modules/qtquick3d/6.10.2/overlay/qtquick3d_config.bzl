"""Generated configuration and tracepoint headers for @qtquick3d."""

load("@qt//:qt_tools.bzl", "qt_write_file")

_QTQUICK3D_TRACEPOINTS_P_H = """#ifndef QT_QUICK3D_TRACEPOINTS_P_H
#define QT_QUICK3D_TRACEPOINTS_P_H

#include <QtCore/private/qtrace_p.h>

#endif // QT_QUICK3D_TRACEPOINTS_P_H
"""

def declare_qtquick3d_config_targets():
    """Declares generated tracepoint headers and QML registration stubs for @qtquick3d."""
    qt_write_file(
        name = "gen_qtquick3d_tracepoints_p_h",
        out = "generated_config/qtquick3d_tracepoints_p.h",
        content = _QTQUICK3D_TRACEPOINTS_P_H,
    )
    qt_write_file(
        name = "gen_quick3d_qmltypereg_cpp",
        out = "generated_config/quick3d_qmltyperegistrations.cpp",
        content = """\
#include <QtQuick3D/private/qtquick3dglobal_p.h>
QT_BEGIN_NAMESPACE
void Q_QUICK3D_EXPORT qml_register_types_QtQuick3D() {}
QT_END_NAMESPACE
""",
    )
