/*
 * Minimal stand-in for MoltenVK's <MoltenVK/mvk_vulkan.h>.
 *
 * Qt's cocoa QPA plugin includes this header when QT_FEATURE_vulkan is on.
 * MoltenVK itself is not linked: Qt dlopen()s the Vulkan loader at runtime,
 * so all the plugin needs is the Khronos headers with the Apple surface
 * platforms enabled (VK_MVK_macos_surface and VK_EXT_metal_surface).
 */

#ifndef QT_BAZEL_MVK_VULKAN_H
#define QT_BAZEL_MVK_VULKAN_H

#ifndef VK_USE_PLATFORM_MACOS_MVK
#define VK_USE_PLATFORM_MACOS_MVK 1
#endif
#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT 1
#endif

#include <vulkan/vulkan.h>

#endif  // QT_BAZEL_MVK_VULKAN_H
