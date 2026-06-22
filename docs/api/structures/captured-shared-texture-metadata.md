# CapturedSharedTextureMetadata Object

Extra metadata for a captured shared texture. These fields are provided by Chromium's frame-capture pipeline and may be absent or contain capture-specific values depending on platform, compositor state, and frame timing.

* `captureUpdateRect` [Rectangle](rectangle.md) (optional) - Updated area of frame, can be considered as the `dirty` area.
* `regionCaptureRect` [Rectangle](rectangle.md) (optional) - May reflect the frame's contents origin if region capture is used internally.
* `sourceSize` [Size](size.md) (optional) - Full size of the source frame.
* `frameCount` number (optional) - The increasing count of captured frame. May contain gaps if frames are dropped between two consecutively received frames.
