// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/capture/shared_texture_frame_producer.h"

#include <utility>

#include "base/check.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-shared.h"
#include "shell/browser/capture/shared_texture_frame_info.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace electron {

SharedTextureFrameProducer::SharedTextureFrameProducer(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {
  CHECK(delegate_);
}

SharedTextureFrameProducer::~SharedTextureFrameProducer() {
  Stop();
}

void SharedTextureFrameProducer::Start(
    const SharedTextureFrameProducerOptions& options) {
  options_ = options;
  drop_next_frame_ = false;

  if (!EnsureCapturer())
    return;

  if (!ApplyCaptureSettings())
    return;

  capturer_count_ = web_contents_->IncrementCapturerCount(
      gfx::Size(), options_.stay_hidden, options_.stay_awake,
      options_.is_activity);

  if (!video_capturer_started_) {
    video_capturer_->Start(
        this, viz::mojom::BufferFormatPreference::kPreferMappableSharedImage);
    video_capturer_started_ = true;
  }
}

void SharedTextureFrameProducer::Pause() {
  if (video_capturer_)
    video_capturer_->SetMinCapturePeriod(base::Seconds(3600));
  capturer_count_.RunAndReset();
}

void SharedTextureFrameProducer::Stop() {
  if (video_capturer_)
    video_capturer_->Stop();
  video_capturer_.reset();
  video_capturer_started_ = false;
  drop_next_frame_ = false;
  capturer_count_.RunAndReset();
}

void SharedTextureFrameProducer::RequestRefreshFrame() {
  if (video_capturer_)
    video_capturer_->RequestRefreshFrame();
}

void SharedTextureFrameProducer::DropNextFrame() {
  drop_next_frame_ = true;
}

bool SharedTextureFrameProducer::SetFrameRate(int fps) {
  options_.fps = fps;
  if (!video_capturer_)
    return true;

  video_capturer_->SetMinCapturePeriod(base::Hertz(options_.fps));
  return true;
}

bool SharedTextureFrameProducer::EnsureCapturer() {
  auto* primary_frame = web_contents_->GetPrimaryMainFrame();
  auto* render_widget_host =
      primary_frame ? primary_frame->GetRenderWidgetHost() : nullptr;
  auto* view = render_widget_host ? render_widget_host->GetView() : nullptr;
  if (!view) {
    stats_.last_error = "WebContents has no RenderWidgetHostView";
    delegate_->OnSharedTextureError(stats_.last_error);
    return false;
  }

  if (video_capturer_)
    return true;

  video_capturer_ = view->CreateVideoCapturer();
  if (!video_capturer_) {
    stats_.last_error = "Failed to create frame sink video capturer";
    delegate_->OnSharedTextureError(stats_.last_error);
    return false;
  }
  ++stats_.native_capturer_create_count;

  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetAnimationFpsLockIn(false, 1);
  return true;
}

bool SharedTextureFrameProducer::ApplyCaptureSettings() {
  auto* primary_frame = web_contents_->GetPrimaryMainFrame();
  auto* render_widget_host =
      primary_frame ? primary_frame->GetRenderWidgetHost() : nullptr;
  auto* view = render_widget_host ? render_widget_host->GetView() : nullptr;
  if (!view) {
    stats_.last_error = "WebContents has no RenderWidgetHostView";
    delegate_->OnSharedTextureError(stats_.last_error);
    return false;
  }

  SetFrameRate(options_.fps);
  video_capturer_->SetFormat(options_.pixel_format);
  const gfx::Size view_size = gfx::ToRoundedSize(gfx::ScaleSize(
      gfx::SizeF(view->GetViewBounds().size()), view->GetDeviceScaleFactor()));
  if (view_size.IsEmpty()) {
    stats_.last_error = "WebContents RenderWidgetHostView has empty bounds";
    delegate_->OnSharedTextureError(stats_.last_error);
    return false;
  }

  switch (options_.output_mode) {
    case SharedTextureFrameProducerOptions::OutputMode::kSourceSize:
      // Unlike OSR, normal onscreen frame sinks need exact constraints here to
      // reliably produce requested refresh frames.
      video_capturer_->SetResolutionConstraints(view_size, view_size, true);
      break;
    case SharedTextureFrameProducerOptions::OutputMode::kFixed:
      video_capturer_->SetResolutionConstraints(
          options_.output_size, options_.output_size,
          options_.preserve_aspect_ratio);
      break;
    case SharedTextureFrameProducerOptions::OutputMode::kMaxBounds:
      video_capturer_->SetResolutionConstraints(
          gfx::Size(1, 1), options_.output_size,
          options_.preserve_aspect_ratio);
      break;
  }
  return true;
}

void SharedTextureFrameProducer::ReleaseCallbacks(
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));
  callbacks_remote->Done();
}

void SharedTextureFrameProducer::FailFrame(
    std::string message,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  stats_.last_error = std::move(message);
  ReleaseCallbacks(std::move(callbacks));
  delegate_->OnSharedTextureError(stats_.last_error);
}

void SharedTextureFrameProducer::OnFrameCaptured(
    ::media::mojom::VideoBufferHandlePtr data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  if (drop_next_frame_) {
    drop_next_frame_ = false;
    ++stats_.dropped_frame_count;
    ReleaseCallbacks(std::move(callbacks));
    return;
  }

  if (!data || !info) {
    FailFrame("Captured shared texture frame data is missing",
              std::move(callbacks));
    return;
  }

  if (!data->is_gpu_memory_buffer_handle()) {
    FailFrame("Captured frame is not backed by a GPU memory buffer",
              std::move(callbacks));
    return;
  }

  auto& orig_handle = data->get_gpu_memory_buffer_handle();
  if (orig_handle.is_null()) {
    FailFrame("Captured GPU memory buffer handle is null",
              std::move(callbacks));
    return;
  }

  auto gmb_handle = orig_handle.Clone();

  CapturedSharedTextureValue texture;
  PopulateSharedTextureValueFromFrame(&texture, gmb_handle, *info, content_rect,
                                      content::WidgetType::kFrame);
  stats_.last_frame_coded_size = texture.coded_size;
  stats_.last_frame_timestamp = texture.timestamp;

  texture.releaser_holder = new SharedTextureReleaserHolder(
      std::move(gmb_handle), std::move(callbacks),
      base::BindOnce(&SharedTextureFrameProducer::OnTextureReleased,
                     weak_factory_.GetWeakPtr()));

  auto* releaser_holder = texture.releaser_holder.get();
  if (!delegate_->OnSharedTextureFrame(std::move(texture))) {
    ++stats_.unexpected_frame_done_count;
    releaser_holder->on_release.Reset();
    delete releaser_holder;
    return;
  }

  ++stats_.captured_frame_count;
}

void SharedTextureFrameProducer::OnTextureReleased() {
  ++stats_.released_frame_count;
  delegate_->OnSharedTextureFrameReleased();
}

}  // namespace electron
