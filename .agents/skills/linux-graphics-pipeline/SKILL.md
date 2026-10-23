---
name: linux-graphics-pipeline
description: Linux 3D graphics bridge engineering: OpenGL 4.1 Core Profile CGL interception, Vulkan/Zink translation, Wayland buffer presentation, and Mesa driver overrides.
risk: medium
source: project
date_added: '2026-10-24'
---

## Use this skill when
- Configuring or debugging graphics pipelines for Roblox running inside Darling.
- Maintaining CGL hooks in `shims/gl_profile.c` (`CGLChoosePixelFormat`, `kCGLOGLPVersion_3_2_Core`).
- Setting up and testing Mesa Zink (OpenGL over Vulkan) or native OpenGL drivers.
- Debugging GPU context failures, shader compilation errors, or blank window rendering.
- Verifying `vulkan-icd-loader` and hardware acceleration on Wayland/X11.

## Do not use this skill when
- The task is purely QSS styling of the launcher GUI (use `qt6-desktop-expert`).
- The task is general C++ logic without GPU/graphics API implications.

## Graphics Architecture for Roblox under Darling

### 1. OpenGL 4.1 Core Profile Pipeline
- macOS Roblox requests an OpenGL legacy context by default on older macOS versions, which fails on modern Linux Mesa drivers.
- **Interception (`shims/gl_profile.c`)**:
  - Intercepts `CGLChoosePixelFormat` attribute array.
  - Injects `kCGLOGLPVersion_3_2_Core` (0x4a) to force macOS/Darling CGL to create an OpenGL 3.2+ / 4.1 Core Profile context.
  - Sets Roblox FastFlag: `FFlagDebugGraphicsPreferOpenGL=true`.

### 2. Vulkan & Mesa Zink Pipeline
- Provides higher performance and compatibility with modern AMD, Intel, and NVIDIA Vulkan drivers.
- **Environment Configuration**:
  ```bash
  export MACOBLOX_GRAPHICS_BACKEND=vulkan
  export MESA_LOADER_DRIVER_OVERRIDE=zink
  export GALLIUM_DRIVER=zink
  ```
- FastFlag configuration: `FFlagDebugGraphicsPreferVulkan=true`.
- Requires host Linux package `vulkan-icd-loader` and distribution Vulkan drivers (`vulkan-radeon`, `vulkan-intel`, `nvidia-utils`).

### 3. Wayland Buffer Sharing & Presentation
- Ensure Qt6 Wayland buffer plugins (`Qt6VulkanServerBufferPlugin`) are available.
- Wayland presentation requires explicit synchronization to avoid tearing or stutter.
- For hybrid GPUs (NVIDIA Optimus / AMD Prime):
  - Check `DRI_PRIME=1` or `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`.

### 4. Diagnostics & Troubleshooting Playbook
1. **Black Screen / Blank Window**:
   - Check `glxinfo | grep "OpenGL core profile version"`.
   - Verify that Darling has access to `/dev/dri/renderD128` and `/dev/dri/card0`.
2. **Context Creation Failure (GLXBadFBConfig / CGL Error)**:
   - Verify `libgl_profile.dylib` is properly specified in `DYLD_INSERT_LIBRARIES`.
3. **Mesa Zink Crash**:
   - Verify `vkcube` runs cleanly on the host system first.
   - Run with `MESA_DEBUG=1 ZINK_DEBUG=validation`.
