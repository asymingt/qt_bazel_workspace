#include <QCoreApplication>
#include <QFile>
#include <rhi/qshaderbaker.h>
#include <cassert>
#include <cstdio>

#include <rhi/qshader.h>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    assert(argc == 2);

    // 1. Load the .qsb produced by the qsb host tool in the genrule.
    QFile f(QString::fromLocal8Bit(argv[1]));
    assert(f.open(QIODevice::ReadOnly));
    QShader baked = QShader::fromSerialized(f.readAll());
    assert(baked.isValid());
    assert(baked.stage() == QShader::FragmentStage);
    assert(baked.availableShaders().size() >= 5);  // spirv + 3x glsl + hlsl + msl
    assert(!baked.shader(QShaderKey(QShader::GlslShader, QShaderVersion(120))).shader().isEmpty());
    assert(!baked.shader(QShaderKey(QShader::MslShader, QShaderVersion(12))).shader().isEmpty());

    // 2. Use the QtShaderTools library in-process (glslang + SPIRV-Cross).
    QShaderBaker baker;
    baker.setSourceString(
        "#version 440\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0, 0.5, 0.25, 1.0); }\n",
        QShader::FragmentStage);
    baker.setGeneratedShaderVariants({QShader::StandardShader});
    baker.setGeneratedShaders({
        {QShader::SpirvShader, QShaderVersion(100)},
        {QShader::GlslShader, QShaderVersion(100, QShaderVersion::GlslEs)},
        {QShader::HlslShader, QShaderVersion(50)},
    });
    QShader s = baker.bake();
    if (!s.isValid()) {
        std::fprintf(stderr, "bake failed: %s\n", qPrintable(baker.errorMessage()));
        return 1;
    }
    assert(s.description().outputVariables().size() == 1);
    return 0;
}
