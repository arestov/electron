// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/capture/shared_texture_types.h"

namespace electron {

SharedTextureNativePixmapPlaneInfo::~SharedTextureNativePixmapPlaneInfo() =
    default;
SharedTextureNativePixmapPlaneInfo::SharedTextureNativePixmapPlaneInfo(
    const SharedTextureNativePixmapPlaneInfo& other) = default;
SharedTextureNativePixmapPlaneInfo::SharedTextureNativePixmapPlaneInfo(
    uint32_t stride,
    uint64_t offset,
    uint64_t size,
    int fd)
    : stride(stride), offset(offset), size(size), fd(fd) {}

}  // namespace electron
