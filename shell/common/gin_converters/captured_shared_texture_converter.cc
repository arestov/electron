// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/gin_converters/captured_shared_texture_converter.h"

#include "gin/dictionary.h"
#include "v8-external.h"
#include "v8-function.h"

#include <string>

#include "base/containers/to_vector.h"
#include "base/task/single_thread_task_runner.h"
#if BUILDFLAG(IS_LINUX)
#include "base/strings/string_number_conversions.h"
#endif
#include "shell/common/gin_converters/gfx_converter.h"
#include "shell/common/gin_converters/optional_converter.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"

namespace gin {

namespace {

std::string CapturedVideoPixelFormatToString(media::VideoPixelFormat format) {
  switch (format) {
    case media::PIXEL_FORMAT_ARGB:
      return "bgra";
    case media::PIXEL_FORMAT_ABGR:
      return "rgba";
    case media::PIXEL_FORMAT_RGBAF16:
      return "rgbaf16";
    case media::PIXEL_FORMAT_NV12:
      return "nv12";
    default:
      NOTREACHED();
  }
}

std::string CapturedWidgetTypeToString(content::WidgetType type) {
  switch (type) {
    case content::WidgetType::kPopup:
      return "popup";
    case content::WidgetType::kFrame:
      return "frame";
    default:
      NOTREACHED();
  }
}

struct CapturedReleaseHolderMonitor {
  explicit CapturedReleaseHolderMonitor(
      electron::SharedTextureReleaserHolder* holder)
      : holder_(holder) {
    CHECK(holder);
  }

  void ReleaseTexture() {
    delete holder_;
    holder_ = nullptr;
  }

  [[nodiscard]] bool IsTextureReleased() const { return holder_ == nullptr; }

  v8::Persistent<v8::Value>* CreatePersistent(v8::Isolate* isolate,
                                              v8::Local<v8::Value> value) {
    persistent_ = std::make_unique<v8::Persistent<v8::Value>>(isolate, value);
    return persistent_.get();
  }

  void ResetPersistent() const { persistent_->Reset(); }

 private:
  raw_ptr<electron::SharedTextureReleaserHolder> holder_;
  std::unique_ptr<v8::Persistent<v8::Value>> persistent_;
};

}  // namespace

// static
v8::Local<v8::Value> Converter<electron::CapturedSharedTextureValue>::ToV8(
    v8::Isolate* isolate,
    const electron::CapturedSharedTextureValue& val) {
  gin::Dictionary root(isolate, v8::Object::New(isolate));

  auto* monitor = new CapturedReleaseHolderMonitor(val.releaser_holder);

  auto releaser_holder =
      v8::External::New(isolate, monitor, v8::kExternalPointerTypeTagDefault);
  auto releaser_func = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    auto* mon = static_cast<CapturedReleaseHolderMonitor*>(
        info.Data().As<v8::External>()->Value(
            v8::kExternalPointerTypeTagDefault));
    mon->ReleaseTexture();
  };
  auto releaser =
      v8::Function::New(isolate->GetCurrentContext(), releaser_func,
                        releaser_holder)
          .ToLocalChecked();

  root.Set("release", releaser);

  gin::Dictionary dict(isolate, v8::Object::New(isolate));
  dict.Set("pixelFormat", CapturedVideoPixelFormatToString(val.pixel_format));
  dict.Set("codedSize", val.coded_size);
  dict.Set("visibleRect", val.visible_rect);
  dict.Set("contentRect", val.content_rect);
  dict.Set("timestamp", val.timestamp);
  dict.Set("colorSpace", val.color_space);
  dict.Set("widgetType", CapturedWidgetTypeToString(val.widget_type));

  gin::Dictionary metadata(isolate, v8::Object::New(isolate));
  metadata.Set("captureUpdateRect", val.capture_update_rect);
  metadata.Set("regionCaptureRect", val.region_capture_rect);
  metadata.Set("sourceSize", val.source_size);
  metadata.Set("frameCount", val.frame_count);
  dict.Set("metadata", ConvertToV8(isolate, metadata));

  gin::Dictionary shared_texture(isolate, v8::Object::New(isolate));
#if BUILDFLAG(IS_WIN)
  shared_texture.Set(
      "ntHandle",
      electron::Buffer::Copy(
          isolate, base::byte_span_from_ref(val.shared_texture_handle))
          .ToLocalChecked());
#elif BUILDFLAG(IS_MAC)
  shared_texture.Set(
      "ioSurface",
      electron::Buffer::Copy(
          isolate, base::byte_span_from_ref(val.shared_texture_handle))
          .ToLocalChecked());
#elif BUILDFLAG(IS_LINUX)
  gin::Dictionary native_pixmap(isolate, v8::Object::New(isolate));
  auto v8_planes = base::ToVector(val.planes, [isolate](const auto& plane) {
    gin::Dictionary v8_plane(isolate, v8::Object::New(isolate));
    v8_plane.Set("stride", plane.stride);
    v8_plane.Set("offset", plane.offset);
    v8_plane.Set("size", plane.size);
    v8_plane.Set("fd", plane.fd);
    return v8_plane;
  });
  native_pixmap.Set("planes", v8_planes);
  native_pixmap.Set("modifier", base::NumberToString(val.modifier));
  native_pixmap.Set("supportsZeroCopyWebGpuImport",
                    val.supports_zero_copy_webgpu_import);
  shared_texture.Set("nativePixmap", ConvertToV8(isolate, native_pixmap));
#endif

  dict.Set("handle", ConvertToV8(isolate, shared_texture));
  root.Set("textureInfo", ConvertToV8(isolate, dict));
  auto root_local = ConvertToV8(isolate, root);

  auto* tex_persistent = monitor->CreatePersistent(isolate, releaser);
  tex_persistent->SetWeak(
      monitor,
      [](const v8::WeakCallbackInfo<CapturedReleaseHolderMonitor>& data) {
        auto* monitor = data.GetParameter();
        if (!monitor->IsTextureReleased()) {
          data.SetSecondPassCallback([](
              const v8::WeakCallbackInfo<CapturedReleaseHolderMonitor>& data) {
            static std::once_flag flag;
            std::call_once(flag, [=] {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce([] {
                    electron::util::EmitWarning(
                        "Captured shared texture was garbage collected before "
                        "calling `release()`. `texture.release()` must be "
                        "called explicitly after the external renderer is done "
                        "with the texture. Otherwise, future captures may be "
                        "blocked while the underlying frame remains in use.",
                        "CapturedSharedTextureNotReleased");
                  }));
            });
          });
        }
        monitor->ResetPersistent();
        delete monitor;
      },
      v8::WeakCallbackType::kParameter);

  return root_local;
}

}  // namespace gin
