// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_INFO_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_INFO_H_

#include "build/build_config.h"
#include "content/public/common/widget_type.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace electron {

template <typename TextureValue>
void PopulateSharedTextureValueFromFrame(
    TextureValue* texture,
    const gfx::GpuMemoryBufferHandle& gmb_handle,
    const media::mojom::VideoFrameInfo& info,
    const gfx::Rect& content_rect,
    content::WidgetType widget_type) {
  texture->pixel_format = info.pixel_format;
  texture->coded_size = info.coded_size;
  texture->visible_rect = info.visible_rect;
  texture->content_rect = content_rect;
  texture->color_space = info.color_space;
  texture->timestamp = info.timestamp.InMicroseconds();
  texture->frame_count = info.metadata.capture_counter.value_or(0);
  texture->capture_update_rect = info.metadata.capture_update_rect;
  texture->source_size = info.metadata.source_size;
  texture->region_capture_rect = info.metadata.region_capture_rect;
  texture->widget_type = widget_type;

#if BUILDFLAG(IS_WIN)
  texture->shared_texture_handle =
      reinterpret_cast<uintptr_t>(gmb_handle.dxgi_handle().buffer_handle());
#elif BUILDFLAG(IS_APPLE)
  texture->shared_texture_handle =
      reinterpret_cast<uintptr_t>(gmb_handle.io_surface().get());
#elif BUILDFLAG(IS_LINUX)
  const auto& native_pixmap = gmb_handle.native_pixmap_handle();
  texture->modifier = native_pixmap.modifier;
  texture->supports_zero_copy_webgpu_import =
      native_pixmap.supports_zero_copy_webgpu_import;
  for (const auto& plane : native_pixmap.planes) {
    texture->planes.emplace_back(plane.stride, plane.offset, plane.size,
                                 plane.fd.get());
  }
#endif
}

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_INFO_H_
