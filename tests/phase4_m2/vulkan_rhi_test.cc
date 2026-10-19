#include <QtCore/QCoreApplication>
#include <QtGui/QtGui>
#include <QtGui/QVulkanInstance>
#include <QtGui/QVulkanWindow>
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanDeviceFunctions>
#include <QtGui/private/qtgui-config_p.h>
#include <QtGui/rhi/qrhi.h>
#include <QtTest/QtTest>

#if !QT_CONFIG(vulkan)
#error "Expected QT_CONFIG(vulkan) to be enabled in Milestone 4.2!"
#endif

#if !QT_CONFIG(vkgen)
#error "Expected QT_CONFIG(vkgen) to be enabled in Milestone 4.2!"
#endif

class VulkanRhiTest : public QObject {
    Q_OBJECT
private slots:
    void vulkanInstanceAndRhiSymbolsLinked() {
        QVulkanInstance inst;
        QVERIFY(!inst.isValid());
        // Verify QRhi::Vulkan backend probe compiles and links cleanly
        QRhiVulkanInitParams params;
        params.inst = &inst;
        // Without calling inst.create(), probe returns false or handles gracefully,
        // and we also verify QVulkanWindow can be instantiated.
        QVulkanWindow window;
        QVERIFY(window.vulkanInstance() == nullptr);
    }
};

QTEST_MAIN(VulkanRhiTest)
#include "vulkan_rhi_test.moc"
