// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_
#define ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/video_types.h"
#include "shell/browser/capture/captured_shared_texture.h"
#include "shell/browser/capture/shared_texture_frame_producer.h"

namespace content {
class WebContents;
}  // namespace content

namespace electron {

class SharedTextureCaptureController
    : public SharedTextureFrameProducer::Delegate {
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

  bool OnSharedTextureFrame(CapturedSharedTextureValue texture) override;
  void OnSharedTextureError(std::string message) override;
  void OnSharedTextureFrameReleased() override;

  void RejectPending(std::string message);
  void OnTimeout(uint64_t capture_id);

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<SharedTextureFrameProducer> producer_;
  State state_ = State::kIdle;
  uint64_t capture_id_ = 0;
  CompletionCallback callback_;

  base::WeakPtrFactory<SharedTextureCaptureController> weak_factory_{this};
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_CAPTURE_SHARED_TEXTURE_CAPTURE_CONTROLLER_H_
