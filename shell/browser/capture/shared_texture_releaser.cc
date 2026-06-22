// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/capture/shared_texture_releaser.h"

#include <utility>

namespace electron {

SharedTextureReleaserHolder::~SharedTextureReleaserHolder() {
  if (on_release)
    std::move(on_release).Run();
}

SharedTextureReleaserHolder::SharedTextureReleaserHolder(
    gfx::GpuMemoryBufferHandle gmb_handle,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        releaser)
    : gmb_handle(std::move(gmb_handle)), releaser(std::move(releaser)) {}

SharedTextureReleaserHolder::SharedTextureReleaserHolder(
    gfx::GpuMemoryBufferHandle gmb_handle,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        releaser,
    base::OnceClosure on_release)
    : gmb_handle(std::move(gmb_handle)),
      releaser(std::move(releaser)),
      on_release(std::move(on_release)) {}

}  // namespace electron
