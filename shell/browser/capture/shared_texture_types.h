// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_TYPES_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_TYPES_H_

#include <cstdint>

namespace electron {

struct SharedTextureNativePixmapPlaneInfo {
  // The strides and offsets in bytes to be used when accessing the buffers
  // via a memory mapping. One per plane per entry. Size in bytes of the
  // plane is necessary to map the buffers.
  uint32_t stride;
  uint64_t offset;
  uint64_t size;

  // File descriptor for the underlying memory object (usually dmabuf).
  int fd;

  SharedTextureNativePixmapPlaneInfo() = delete;
  ~SharedTextureNativePixmapPlaneInfo();
  SharedTextureNativePixmapPlaneInfo(
      const SharedTextureNativePixmapPlaneInfo& other);
  SharedTextureNativePixmapPlaneInfo(uint32_t stride,
                                     uint64_t offset,
                                     uint64_t size,
                                     int fd);
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_TYPES_H_
