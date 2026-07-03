// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_PRODUCER_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_PRODUCER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/common/widget_type.h"
#include "ui/gfx/geometry/size.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "shell/browser/capture/captured_shared_texture.h"

namespace content {
class WebContents;
}  // namespace content

namespace electron {

struct SharedTextureFrameProducerOptions {
  enum class OutputMode {
    kSourceSize,
    kFixed,
    kMaxBounds,
  };

  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_ARGB;
  bool stay_hidden = true;
  bool stay_awake = false;
  int fps = 60;
  bool is_activity = true;
  OutputMode output_mode = OutputMode::kSourceSize;
  gfx::Size output_size;
  bool preserve_aspect_ratio = true;
};

struct SharedTextureFrameProducerStats {
  int native_capturer_create_count = 0;
  int captured_frame_count = 0;
  int released_frame_count = 0;
  int unexpected_frame_done_count = 0;
  int dropped_frame_count = 0;
  gfx::Size last_frame_coded_size;
  int64_t last_frame_timestamp = 0;
  std::string last_error;
};

class SharedTextureFrameProducer
    : public viz::mojom::FrameSinkVideoConsumer {
 public:
  class Delegate {
   public:
    virtual bool OnSharedTextureFrame(CapturedSharedTextureValue texture) = 0;
    virtual void OnSharedTextureError(std::string message) = 0;
    virtual void OnSharedTextureFrameReleased() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  SharedTextureFrameProducer(content::WebContents* web_contents,
                             Delegate* delegate);
  ~SharedTextureFrameProducer() override;

  SharedTextureFrameProducer(const SharedTextureFrameProducer&) = delete;
  SharedTextureFrameProducer& operator=(const SharedTextureFrameProducer&) =
      delete;

  void Start(const SharedTextureFrameProducerOptions& options);
  void Pause();
  void Stop();
  void RequestRefreshFrame();
  void DropNextFrame();
  bool SetFrameRate(int fps);

  [[nodiscard]] bool is_started() const { return video_capturer_started_; }
  [[nodiscard]] const SharedTextureFrameProducerStats& stats() const {
    return stats_;
  }

 private:
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

  bool EnsureCapturer();
  bool ApplyCaptureSettings();
  void ReleaseCallbacks(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks);
  void FailFrame(
      std::string message,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks);
  void OnTextureReleased();

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  bool video_capturer_started_ = false;
  bool drop_next_frame_ = false;
  SharedTextureFrameProducerOptions options_;
  base::ScopedClosureRunner capturer_count_;
  SharedTextureFrameProducerStats stats_;

  base::WeakPtrFactory<SharedTextureFrameProducer> weak_factory_{this};
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_FRAME_PRODUCER_H_
