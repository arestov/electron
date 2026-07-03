// Copyright (c) 2026 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_COMMON_GIN_CONVERTERS_CAPTURED_SHARED_TEXTURE_CONVERTER_H_
#define ELECTRON_SHELL_COMMON_GIN_CONVERTERS_CAPTURED_SHARED_TEXTURE_CONVERTER_H_

#include "gin/converter.h"
#include "shell/browser/capture/captured_shared_texture.h"

namespace gin {

template <>
struct Converter<electron::CapturedSharedTextureValue> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      const electron::CapturedSharedTextureValue& val);
};

}  // namespace gin

#endif  // ELECTRON_SHELL_COMMON_GIN_CONVERTERS_CAPTURED_SHARED_TEXTURE_CONVERTER_H_
