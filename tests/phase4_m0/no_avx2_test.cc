#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
// Follow PLT / IAT indirect jumps (`ff 25 <disp32>`) or direct jumps (`e9 <disp32>`)
// so we inspect the actual function body inside libgui_shared.
static const uint8_t *resolveFunctionBody(const void *fnPtr) {
    const auto *p = reinterpret_cast<const uint8_t *>(fnPtr);
    for (int hop = 0; hop < 4; ++hop) {
        // Optional ENDBR64 (f3 0f 1e fa)
        if (p[0] == 0xf3 && p[1] == 0x0f && p[2] == 0x1e && p[3] == 0xfa) {
            p += 4;
        }
        if (p[0] == 0xff && p[1] == 0x25) {
            int32_t disp = 0;
            std::memcpy(&disp, p + 2, sizeof(disp));
            const void *const *slot =
                reinterpret_cast<const void *const *>(p + 6 + disp);
            p = reinterpret_cast<const uint8_t *>(*slot);
            continue;
        }
        if (p[0] == 0xe9) {
            int32_t disp = 0;
            std::memcpy(&disp, p + 1, sizeof(disp));
            p = p + 5 + disp;
            continue;
        }
        break;
    }
    return p;
}

// Check that a baseline QtGui function (compiled as part of general Gui sources,
// not a dedicated SIMD TU) does not contain VEX-encoded AVX/AVX2 instructions
// that would SIGILL on pre-AVX x86_64 CPUs or Rosetta 2.
static void assertNoVexInBaselineFunction(const char *name, const void *fnPtr) {
    const uint8_t *code = resolveFunctionBody(fnPtr);
    // Scan up to the first `ret` (0xc3) or 96 bytes.
    for (int i = 0; i < 96; ++i) {
        if (code[i] == 0xc3) {
            break;
        }
        // Specific VEX sequences emitted by Clang in QTransform::fromScale /
        // QTransform::fromTranslate when -march=core-avx2 is active:
        //   c5 fb 11 (`vmovsd`), c5 e8 57 (`vxorps`), c5 f8 11 (`vmovups`),
        //   c5 f9 2e (`vucomisd`), c5 f8 77 (`vzeroupper`).
        const bool isVex =
            (i + 2 < 96) &&
            ((code[i] == 0xc5 && code[i + 1] == 0xfb && code[i + 2] == 0x11) ||
             (code[i] == 0xc5 && code[i + 1] == 0xe8 && code[i + 2] == 0x57) ||
             (code[i] == 0xc5 && code[i + 1] == 0xf8 && code[i + 2] == 0x11) ||
             (code[i] == 0xc5 && code[i + 1] == 0xf9 && code[i + 2] == 0x2e) ||
             (code[i] == 0xc5 && code[i + 1] == 0xf8 && code[i + 2] == 0x77));
        if (isVex) {
            std::fprintf(stderr,
                         "FAIL: %s contains VEX-encoded AVX instruction at offset +%d "
                         "(%02x %02x %02x)\n",
                         name, i, code[i], code[i + 1], code[i + 2]);
            assert(!isVex);
        }
    }
}
#endif

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Call QTransform functions first to ensure lazy PLT resolution has completed.
    QTransform t = QTransform::fromScale(2.0, 3.0) * QTransform::fromTranslate(4.0, 5.0);
    assert(t.isScaling());

#if defined(__x86_64__) || defined(_M_X64)
    QTransform (*fromScaleFn)(qreal, qreal) = &QTransform::fromScale;
    QTransform (*fromTranslateFn)(qreal, qreal) = &QTransform::fromTranslate;
    assertNoVexInBaselineFunction("QTransform::fromScale",
                                  reinterpret_cast<const void *>(fromScaleFn));
    assertNoVexInBaselineFunction("QTransform::fromTranslate",
                                  reinterpret_cast<const void *>(fromTranslateFn));
#endif

    // Exercise QImage + QPainter raster draw helpers.
    QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
    img.fill(0xff112233u);
    {
        QPainter p(&img);
        p.setTransform(t);
        p.fillRect(0, 0, 10, 10, QColor(200, 100, 50, 180));
    }
    assert(img.pixelColor(10, 20).alpha() == 255);
    return 0;
}
