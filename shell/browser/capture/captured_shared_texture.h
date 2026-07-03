// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_CAPTURED_SHARED_TEXTURE_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_CAPTURED_SHARED_TEXTURE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "content/public/common/widget_type.h"
#include "media/base/video_types.h"
#include "shell/browser/capture/shared_texture_releaser.h"
#include "shell/browser/capture/shared_texture_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace electron {

struct CapturedSharedTextureValue {
  CapturedSharedTextureValue();
  ~CapturedSharedTextureValue();
  CapturedSharedTextureValue(const CapturedSharedTextureValue& other);

  content::WidgetType widget_type = content::WidgetType::kFrame;
  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_UNKNOWN;
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Rect content_rect;
  gfx::ColorSpace color_space;
  std::optional<gfx::Rect> capture_update_rect;
  std::optional<gfx::Size> source_size;
  std::optional<gfx::Rect> region_capture_rect;
  int64_t timestamp = 0;
  int64_t frame_count = 0;
  raw_ptr<SharedTextureReleaserHolder> releaser_holder = nullptr;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  uintptr_t shared_texture_handle = 0;
#elif BUILDFLAG(IS_LINUX)
  std::vector<SharedTextureNativePixmapPlaneInfo> planes;
  uint64_t modifier = 0;
  bool supports_zero_copy_webgpu_import = false;
#endif
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_CAPTURED_SHARED_TEXTURE_H_
