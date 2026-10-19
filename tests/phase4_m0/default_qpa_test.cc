#include <QGuiApplication>
#include <cassert>
#include <cstdio>
#include <cstring>

int main(int, char *[]) {
    // Clear environment variables that override the compiled-in default QPA.
    qunsetenv("QT_QPA_PLATFORM");
    qunsetenv("DISPLAY");
    qunsetenv("WAYLAND_DISPLAY");
    qunsetenv("XDG_SESSION_TYPE");

#if defined(Q_OS_LINUX)
    // In QGuiApplicationPrivate::createPlatformIntegration() (qguiapplication.cpp:1557):
    //   const bool xcbIsDefault = platformName.startsWith("xcb");
    // `-title <arg>` (with a single dash, as opposed to `-qwindowtitle`) is ONLY
    // consumed from argv when `xcbIsDefault` is true at the point `-title` is parsed.
    // Passing `-title XcbDefaultCheck` BEFORE `-platform offscreen` therefore tests
    // that QT_QPA_DEFAULT_PLATFORM_NAME is compiled as "xcb" while still running
    // headlessly under the offscreen QPA.
    char arg0[] = "default_qpa_test";
    char arg1[] = "-title";
    char arg2[] = "XcbDefaultCheck";
    char arg3[] = "-platform";
    char arg4[] = "offscreen";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, nullptr};
    int argc = 5;

    QGuiApplication app(argc, argv);
    if (argc != 1) {
        std::fprintf(stderr,
                     "FAIL: expected '-title XcbDefaultCheck' to be consumed when "
                     "QT_QPA_DEFAULT_PLATFORM_NAME is 'xcb', but argc=%d (argv[1]=%s)\n",
                     argc, argc > 1 ? argv[1] : "<null>");
        assert(argc == 1);
    }
#elif defined(Q_OS_MACOS)
    char arg0[] = "default_qpa_test";
    char arg1[] = "-platform";
    char arg2[] = "offscreen";
    char *argv[] = {arg0, arg1, arg2, nullptr};
    int argc = 3;
    QGuiApplication app(argc, argv);
    assert(argc == 1);
#elif defined(Q_OS_WIN)
    char arg0[] = "default_qpa_test";
    char arg1[] = "-platform";
    char arg2[] = "offscreen";
    char *argv[] = {arg0, arg1, arg2, nullptr};
    int argc = 3;
    QGuiApplication app(argc, argv);
    assert(argc == 1);
#endif
    return 0;
}
