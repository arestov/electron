// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/capture/shared_texture_capture_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace electron {

SharedTextureCaptureController::SharedTextureCaptureController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      producer_(std::make_unique<SharedTextureFrameProducer>(web_contents,
                                                             this)) {}

SharedTextureCaptureController::~SharedTextureCaptureController() {
  if (state_ == State::kWaitingForFrame && callback_) {
    std::move(callback_).Run(
        std::nullopt,
        "WebContents was destroyed before a shared texture frame was captured");
  }
  producer_->Stop();
}

void SharedTextureCaptureController::CaptureNext(Options options,
                                                 CompletionCallback callback) {
  if (state_ == State::kWaitingForFrame) {
    std::move(callback).Run(std::nullopt,
                            "A shared texture capture is already pending");
    return;
  }
  if (state_ == State::kFrameInFlight) {
    std::move(callback).Run(
        std::nullopt,
        "A previously captured shared texture has not been released");
    return;
  }

  callback_ = std::move(callback);

  state_ = State::kWaitingForFrame;
  SharedTextureFrameProducerOptions producer_options;
  producer_options.pixel_format = options.pixel_format;
  producer_options.stay_hidden = options.stay_hidden;
  producer_options.stay_awake = options.stay_awake;
  producer_options.fps = 60;
  producer_options.is_activity = true;
  producer_->Start(producer_options);
  if (state_ != State::kWaitingForFrame)
    return;

  const uint64_t capture_id = ++capture_id_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SharedTextureCaptureController::OnTimeout,
                     weak_factory_.GetWeakPtr(), capture_id),
      options.timeout);
  producer_->RequestRefreshFrame();
}

bool SharedTextureCaptureController::IsCapturePendingForTesting() const {
  return state_ == State::kWaitingForFrame;
}

bool SharedTextureCaptureController::HasUnreleasedFrameForTesting() const {
  return state_ == State::kFrameInFlight;
}

int SharedTextureCaptureController::NativeCapturerCreateCountForTesting()
    const {
  return producer_->stats().native_capturer_create_count;
}

int SharedTextureCaptureController::CapturedFrameCountForTesting() const {
  return producer_->stats().captured_frame_count;
}

int SharedTextureCaptureController::UnexpectedFrameDoneCountForTesting() const {
  return producer_->stats().unexpected_frame_done_count;
}

bool SharedTextureCaptureController::OnSharedTextureFrame(
    CapturedSharedTextureValue texture) {
  if (state_ != State::kWaitingForFrame)
    return false;

  producer_->Pause();
  state_ = State::kFrameInFlight;
  std::move(callback_).Run(std::move(texture), std::string());
  return true;
}

void SharedTextureCaptureController::OnSharedTextureError(std::string message) {
  if (state_ == State::kWaitingForFrame)
    RejectPending(std::move(message));
}

void SharedTextureCaptureController::RejectPending(std::string message) {
  state_ = State::kIdle;
  producer_->Pause();
  ++capture_id_;
  if (callback_)
    std::move(callback_).Run(std::nullopt, std::move(message));
}

void SharedTextureCaptureController::OnTimeout(uint64_t capture_id) {
  if (capture_id != capture_id_ || state_ != State::kWaitingForFrame)
    return;
  RejectPending("Timed out while waiting for a shared texture frame");
}

void SharedTextureCaptureController::OnSharedTextureFrameReleased() {
  if (state_ == State::kFrameInFlight)
    state_ = State::kIdle;
}

}  // namespace electron
