// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SHARED_TEXTURE_SUBSCRIPTION_H_
#define ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SHARED_TEXTURE_SUBSCRIPTION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "gin/wrappable.h"
#include "shell/browser/capture/captured_shared_texture.h"
#include "shell/browser/capture/shared_texture_frame_producer.h"
#include "shell/browser/event_emitter_mixin.h"

namespace content {
class WebContents;
}  // namespace content

namespace gin_helper {
class Dictionary;
}  // namespace gin_helper

namespace electron::api {

class SharedTextureSubscription final
    : public gin::Wrappable<SharedTextureSubscription>,
      public gin_helper::EventEmitterMixin<SharedTextureSubscription>,
      public SharedTextureFrameProducer::Delegate {
 public:
  struct Options {
    int fps = 20;
    media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_ARGB;
    bool stay_hidden = true;
    bool stay_awake = false;
  };

  static SharedTextureSubscription* Create(v8::Isolate* isolate,
                                           content::WebContents* web_contents,
                                           Options options);

  static gin::WrapperInfo kWrapperInfo;
  static const char* GetClassName() { return "SharedTextureSubscription"; }
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const gin::WrapperInfo* wrapper_info() const override;
  const char* GetHumanReadableName() const override;

  SharedTextureSubscription(content::WebContents* web_contents,
                            Options options);
  ~SharedTextureSubscription() override;

  SharedTextureSubscription(const SharedTextureSubscription&) = delete;
  SharedTextureSubscription& operator=(const SharedTextureSubscription&) =
      delete;

  void Start();
  void Pause();
  void Resume();
  void Stop();
  v8::Local<v8::Value> GetStats(v8::Isolate* isolate) const;

  [[nodiscard]] bool is_stopped() const { return state_ == State::kStopped; }

 private:
  enum class State {
    kIdle,
    kStreaming,
    kPaused,
    kStopped,
    kError,
  };

  bool OnSharedTextureFrame(CapturedSharedTextureValue texture) override;
  void OnSharedTextureError(std::string message) override;
  void OnSharedTextureFrameReleased() override;

  void EmitStoppedOnce();

  Options options_;
  std::unique_ptr<SharedTextureFrameProducer> producer_;
  State state_ = State::kIdle;
  int in_flight_frames_ = 0;
  int delivered_frame_count_ = 0;
  int dropped_backpressure_frame_count_ = 0;
  int error_count_ = 0;
  bool stopped_emitted_ = false;
};

}  // namespace electron::api

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SHARED_TEXTURE_SUBSCRIPTION_H_
