// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_RELEASER_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_RELEASER_H_

#include "base/functional/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace electron {

struct SharedTextureReleaserHolder {
  SharedTextureReleaserHolder() = delete;
  ~SharedTextureReleaserHolder();
  SharedTextureReleaserHolder(
      gfx::GpuMemoryBufferHandle gmb_handle,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          releaser);
  SharedTextureReleaserHolder(
      gfx::GpuMemoryBufferHandle gmb_handle,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          releaser,
      base::OnceClosure on_release);

  // GpuMemoryBufferHandle, keep the scoped handle alive.
  gfx::GpuMemoryBufferHandle gmb_handle;

  // Releaser, hold this to prevent FrameSinkVideoCapturer from recycling frame.
  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks> releaser;

  base::OnceClosure on_release;
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_RELEASER_H_
