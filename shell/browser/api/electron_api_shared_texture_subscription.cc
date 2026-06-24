// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_shared_texture_subscription.h"

#include <utility>

#include "cppgc/allocation.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "shell/browser/javascript_environment.h"
#include "shell/common/gin_converters/captured_shared_texture_converter.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/wrappable_pointer_tags.h"

namespace electron::api {

gin::WrapperInfo SharedTextureSubscription::kWrapperInfo =
    electron::MakeWrapperInfo(electron::kElectronSharedTextureSubscription);

SharedTextureSubscription::SharedTextureSubscription(
    content::WebContents* web_contents,
    Options options)
    : options_(options),
      producer_(std::make_unique<SharedTextureFrameProducer>(web_contents,
                                                             this)) {}

SharedTextureSubscription::~SharedTextureSubscription() {
  Stop();
}

// static
SharedTextureSubscription* SharedTextureSubscription::Create(
    v8::Isolate* isolate,
    content::WebContents* web_contents,
    Options options) {
  return cppgc::MakeGarbageCollected<SharedTextureSubscription>(
      isolate->GetCppHeap()->GetAllocationHandle(), web_contents,
      std::move(options));
}

void SharedTextureSubscription::Start() {
  if (state_ == State::kStopped || state_ == State::kStreaming)
    return;

  SharedTextureFrameProducerOptions producer_options;
  producer_options.pixel_format = options_.pixel_format;
  producer_options.stay_hidden = options_.stay_hidden;
  producer_options.stay_awake = options_.stay_awake;
  producer_options.fps = options_.fps;
  producer_options.is_activity = false;
  producer_options.output_mode = options_.output_mode;
  producer_options.output_size = options_.output_size;
  producer_options.preserve_aspect_ratio = options_.preserve_aspect_ratio;
  state_ = State::kStreaming;
  producer_->Start(producer_options);
}

void SharedTextureSubscription::Pause() {
  if (state_ != State::kStreaming)
    return;
  producer_->Pause();
  state_ = State::kPaused;
}

void SharedTextureSubscription::Resume() {
  if (state_ != State::kPaused)
    return;
  Start();
}

void SharedTextureSubscription::Stop() {
  if (state_ == State::kStopped)
    return;
  producer_->Stop();
  state_ = State::kStopped;
  EmitStoppedOnce();
}

v8::Local<v8::Value> SharedTextureSubscription::GetStats(
    v8::Isolate* isolate) const {
  gin_helper::Dictionary dict(isolate, v8::Object::New(isolate));
  const auto& producer_stats = producer_->stats();
  dict.Set("capturedFrames", producer_stats.captured_frame_count);
  dict.Set("deliveredFrames", delivered_frame_count_);
  dict.Set("releasedFrames", producer_stats.released_frame_count);
  dict.Set("droppedBackpressureFrames", dropped_backpressure_frame_count_);
  dict.Set("droppedFrames", producer_stats.dropped_frame_count);
  dict.Set("unexpectedFrameDoneCount",
           producer_stats.unexpected_frame_done_count);
  dict.Set("currentInFlightFrames", in_flight_frames_);
  dict.Set("maxInFlightFrames", 1);
  dict.Set("nativeCapturerCreateCount",
           producer_stats.native_capturer_create_count);
  dict.Set("errorCount", error_count_);
  dict.Set("lastError", producer_stats.last_error);
  dict.Set("stopped", state_ == State::kStopped);
  dict.Set("paused", state_ == State::kPaused);
  return dict.GetHandle();
}

bool SharedTextureSubscription::OnSharedTextureFrame(
    CapturedSharedTextureValue texture) {
  if (state_ != State::kStreaming)
    return false;

  if (in_flight_frames_ >= 1) {
    ++dropped_backpressure_frame_count_;
    return false;
  }

  ++in_flight_frames_;
  ++delivered_frame_count_;
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope handle_scope(isolate);
  EmitWithoutEvent("frame", gin::ConvertToV8(isolate, texture));
  return true;
}

void SharedTextureSubscription::OnSharedTextureError(std::string message) {
  ++error_count_;
  state_ = State::kError;
  EmitWithoutEvent("error", message);
  Stop();
}

void SharedTextureSubscription::OnSharedTextureFrameReleased() {
  if (in_flight_frames_ > 0)
    --in_flight_frames_;
}

void SharedTextureSubscription::EmitStoppedOnce() {
  if (stopped_emitted_)
    return;
  stopped_emitted_ = true;
  EmitWithoutEvent("stopped");
}

gin::ObjectTemplateBuilder SharedTextureSubscription::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin_helper::EventEmitterMixin<
             SharedTextureSubscription>::GetObjectTemplateBuilder(isolate)
      .SetMethod("pause", &SharedTextureSubscription::Pause)
      .SetMethod("resume", &SharedTextureSubscription::Resume)
      .SetMethod("stop", &SharedTextureSubscription::Stop)
      .SetMethod("getStats", &SharedTextureSubscription::GetStats);
}

const gin::WrapperInfo* SharedTextureSubscription::wrapper_info() const {
  return &kWrapperInfo;
}

const char* SharedTextureSubscription::GetHumanReadableName() const {
  return "Electron / SharedTextureSubscription";
}

}  // namespace electron::api
