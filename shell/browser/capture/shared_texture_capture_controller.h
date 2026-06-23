// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/common/widget_type.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "shell/browser/capture/captured_shared_texture.h"

namespace content {
class WebContents;
}  // namespace content

namespace electron {

class SharedTextureCaptureController
    : public viz::mojom::FrameSinkVideoConsumer {
 public:
  struct Options {
    base::TimeDelta timeout = base::Milliseconds(250);
    media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_ARGB;
    bool stay_hidden = true;
    bool stay_awake = false;
  };

  using CompletionCallback =
      base::OnceCallback<void(std::optional<CapturedSharedTextureValue>,
                              std::string)>;

  explicit SharedTextureCaptureController(content::WebContents* web_contents);
  ~SharedTextureCaptureController() override;

  SharedTextureCaptureController(const SharedTextureCaptureController&) =
      delete;
  SharedTextureCaptureController& operator=(
      const SharedTextureCaptureController&) = delete;

  void CaptureNext(Options options, CompletionCallback callback);

  bool IsCapturePendingForTesting() const;
  bool HasUnreleasedFrameForTesting() const;
  int NativeCapturerCreateCountForTesting() const;
  int CapturedFrameCountForTesting() const;
  int UnexpectedFrameDoneCountForTesting() const;

 private:
  enum class State {
    kIdle,
    kWaitingForFrame,
    kFrameInFlight,
  };

  void OnFrameCaptured(
      ::media::mojom::VideoBufferHandlePtr data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewCaptureVersion(
      const media::CaptureVersion& capture_version) override {}
  void OnFrameWithEmptyRegionCapture() override {}
  void OnStopped() override {}
  void OnLog(const std::string& message) override {}

  void StopCapture();
  void PauseCapture();
  void RejectPending(std::string message);
  void OnTimeout(uint64_t capture_id);
  void OnTextureReleased();

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  bool video_capturer_started_ = false;
  base::ScopedClosureRunner capturer_count_;
  State state_ = State::kIdle;
  uint64_t capture_id_ = 0;
  CompletionCallback callback_;
  int native_capturer_create_count_ = 0;
  int captured_frame_count_ = 0;
  int unexpected_frame_done_count_ = 0;

  base::WeakPtrFactory<SharedTextureCaptureController> weak_factory_{this};
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_
