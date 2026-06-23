// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/capture/shared_texture_capture_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-shared.h"
#include "shell/browser/capture/shared_texture_frame_info.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace electron {

SharedTextureCaptureController::SharedTextureCaptureController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

SharedTextureCaptureController::~SharedTextureCaptureController() {
  if (state_ == State::kWaitingForFrame && callback_) {
    std::move(callback_).Run(
        std::nullopt,
        "WebContents was destroyed before a shared texture frame was captured");
  }
  StopCapture();
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

  auto* primary_frame = web_contents_->GetPrimaryMainFrame();
  auto* render_widget_host =
      primary_frame ? primary_frame->GetRenderWidgetHost() : nullptr;
  auto* view = render_widget_host ? render_widget_host->GetView() : nullptr;
  if (!view) {
    RejectPending("WebContents has no RenderWidgetHostView");
    return;
  }

  if (!video_capturer_) {
    video_capturer_ = view->CreateVideoCapturer();
    if (!video_capturer_) {
      RejectPending("Failed to create frame sink video capturer");
      return;
    }

    video_capturer_->SetAutoThrottlingEnabled(false);
    video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
    video_capturer_->SetAnimationFpsLockIn(false, 1);
  }
  video_capturer_->SetMinCapturePeriod(base::Hertz(60));
  video_capturer_->SetFormat(options.pixel_format);
  // Unlike OSR, normal onscreen frame sinks need exact constraints here to
  // reliably produce a requested refresh frame.
  const gfx::Size view_size = gfx::ToRoundedSize(gfx::ScaleSize(
      gfx::SizeF(view->GetViewBounds().size()), view->GetDeviceScaleFactor()));
  if (view_size.IsEmpty()) {
    RejectPending("WebContents RenderWidgetHostView has empty bounds");
    return;
  }
  video_capturer_->SetResolutionConstraints(view_size, view_size, true);
  capturer_count_ = web_contents_->IncrementCapturerCount(
      gfx::Size(), options.stay_hidden, options.stay_awake,
      /*is_activity=*/true);

  state_ = State::kWaitingForFrame;
  const uint64_t capture_id = ++capture_id_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SharedTextureCaptureController::OnTimeout,
                     weak_factory_.GetWeakPtr(), capture_id),
      options.timeout);
  if (!video_capturer_started_) {
    video_capturer_->Start(
        this, viz::mojom::BufferFormatPreference::kPreferMappableSharedImage);
    video_capturer_started_ = true;
  }
  video_capturer_->RequestRefreshFrame();
}

bool SharedTextureCaptureController::IsCapturePendingForTesting() const {
  return state_ == State::kWaitingForFrame;
}

bool SharedTextureCaptureController::HasUnreleasedFrameForTesting() const {
  return state_ == State::kFrameInFlight;
}

void SharedTextureCaptureController::OnFrameCaptured(
    ::media::mojom::VideoBufferHandlePtr data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  if (state_ != State::kWaitingForFrame) {
    mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks_remote(std::move(callbacks));
    callbacks_remote->Done();
    return;
  }

  if (!data || !info) {
    PauseCapture();
    RejectPending("Captured shared texture frame data is missing");
    return;
  }

  if (!data->is_gpu_memory_buffer_handle()) {
    PauseCapture();
    RejectPending("Captured frame is not backed by a GPU memory buffer");
    return;
  }

  auto& orig_handle = data->get_gpu_memory_buffer_handle();
  if (orig_handle.is_null()) {
    PauseCapture();
    RejectPending("Captured GPU memory buffer handle is null");
    return;
  }

  auto gmb_handle = orig_handle.Clone();

  CapturedSharedTextureValue texture;
  PopulateSharedTextureValueFromFrame(&texture, gmb_handle, *info, content_rect,
                                      content::WidgetType::kFrame);

  texture.releaser_holder = new SharedTextureReleaserHolder(
      std::move(gmb_handle), std::move(callbacks),
      base::BindOnce(&SharedTextureCaptureController::OnTextureReleased,
                     weak_factory_.GetWeakPtr()));

  PauseCapture();
  state_ = State::kFrameInFlight;
  std::move(callback_).Run(std::move(texture), std::string());
}

void SharedTextureCaptureController::StopCapture() {
  if (video_capturer_)
    video_capturer_->Stop();
  video_capturer_.reset();
  video_capturer_started_ = false;
  capturer_count_.RunAndReset();
}

void SharedTextureCaptureController::PauseCapture() {
  if (video_capturer_)
    video_capturer_->SetMinCapturePeriod(base::Seconds(3600));
  capturer_count_.RunAndReset();
}

void SharedTextureCaptureController::RejectPending(std::string message) {
  state_ = State::kIdle;
  PauseCapture();
  ++capture_id_;
  if (callback_)
    std::move(callback_).Run(std::nullopt, std::move(message));
}

void SharedTextureCaptureController::OnTimeout(uint64_t capture_id) {
  if (capture_id != capture_id_ || state_ != State::kWaitingForFrame)
    return;
  RejectPending("Timed out while waiting for a shared texture frame");
}

void SharedTextureCaptureController::OnTextureReleased() {
  if (state_ == State::kFrameInFlight)
    state_ = State::kIdle;
}

}  // namespace electron
