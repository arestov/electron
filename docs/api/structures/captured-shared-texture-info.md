# CapturedSharedTextureInfo Object

* `widgetType` string - The widget type of the texture. Can be `frame`.
* `pixelFormat` string - The pixel format of the texture.
  * `rgba` - The texture format is 8-bit unorm RGBA.
  * `bgra` - The texture format is 8-bit unorm BGRA.
  * `rgbaf16` - The texture format is 16-bit float RGBA.
* `codedSize` [Size](size.md) - The full dimensions of the video frame.
* `colorSpace` [ColorSpace](color-space.md) - The color space of the video frame.
* `visibleRect` [Rectangle](rectangle.md) - A subsection of [0, 0, codedSize.width, codedSize.height].
* `contentRect` [Rectangle](rectangle.md) - The region of the video frame that the capturer populated.
* `timestamp` number - The time in microseconds since the capture start.
* `metadata` [CapturedSharedTextureMetadata](captured-shared-texture-metadata.md) - Extra metadata.
* `handle` [SharedTextureHandle](shared-texture-handle.md) - The shared texture handle data.
