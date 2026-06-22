# CapturedSharedTexture Object

* `textureInfo` [CapturedSharedTextureInfo](captured-shared-texture-info.md) - The shared texture info.
* `release` Function - Releases the captured texture resource.

The `CapturedSharedTexture` object cannot be directly passed to another process. Keep the texture lifecycle in the main process, pass only `textureInfo` to consumers, and call `release()` after every consumer has finished using the texture.

Only one unreleased captured shared texture is supported per `WebContents` in this experimental version. A later call to `webContents.captureNextSharedTexture()` rejects until the previous texture is released.
