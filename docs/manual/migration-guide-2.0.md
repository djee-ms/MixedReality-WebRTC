# MixedReality-WebRTC 2.0 migration guide

This migration guide details the steps to migrate a project from version 1.0.x of MixedReality-WebRTC to version 2.0.x.

## Breaking changes overview

- **Standalone track objects** : Audio and video tracks are now standalone objects, distinct from the peer connection they are attached to. Previously the peer connection was encapsulating one instance of each local and remote audio and video track. It is now the responsibility of the user to create the local track objects, keep a reference to them if they need to be modified, and dispose of them. The remote tracks are still managed by the peer connection.

- **Transceiver API** : Audio and video tracks are not attached anymore to the peer connection directly. Instead, a new concept of _transceivers_ is introduced, which encapsulates an audio or video "pipe" between the local and remote peers, through which the track's media data flows. Transceivers are added to the peer connection, then tracks are attached to them.

## C++ project

TODO

## C# project

### Transceiver API

The `PeerConnection` class has 2 new methods to create audio and video transceivers:

- [`PeerConnection.AddAudioTransceiver()`](xref:Microsoft.MixedReality.WebRTC.PeerConnection.AddAudioTransceiver)
- [`PeerConnection.AddVideoTransceiver()`](xref:Microsoft.MixedReality.WebRTC.PeerConnection.AddVideoTransceiver)

Users who previously reasoned in terms of tracks should now use the transceivers workflow. 
The track manipulation APIs have been removed from [`PeerConnection`](xref:Microsoft.MixedReality.WebRTC.PeerConnection):

- `PeerConnection.AddLocalVideoTrackAsync()` was removed; instead users should:
  - Add a transceiver to the peer connection with [`PeerConnection.AddVideoTransceiver()`](xref:Microsoft.MixedReality.WebRTC.PeerConnection.AddVideoTransceiver).
  - Create a local media track with static methods [`LocalVideoTrack.CreateFromDeviceAsync()`](xref:Microsoft.MixedReality.WebRTC.LocalVideoTrack.CreateFromDeviceAsync).
  - Attach the track to the transceiver by setting its [`VideoTransceiver.LocalTrack`](xref:Microsoft.MixedReality.WebRTC.VideoTransceiver.LocalTrack) property.

  ```cs
  var init = new VideoTransceiverInitSettings {
      Name = "my_webcam_track",
      InitialDesiredDirection = Transceiver.Direction.SendReceive,
  };
  transceiver_ = peerConnection_.AddVideoTransceiver(init);
  localTrack_ = LocalVideoTrack.CreateFromDeviceAsync();
  transceiver_.LocalTrack = localTrack_;
  ```

- `PeerConnection.RemoveLocalVideoTrack()` has been removed since the peer connection now supports multiple local video tracks. The new API offers 2 alternatives:
  - Set the transceiver's [`VideoTransceiver.LocalTrack`](xref:Microsoft.MixedReality.WebRTC.VideoTransceiver.LocalTrack) property to `null` instead (in-place switching, no renegotiation). This allows the track to be re-attached later, or another track to be switched to.

  ```cs
  transceiver_.LocalTrack = null;
  ```

  - Change the transceiver's [`VideoTransceiver.DesiredDirection`](xref:Microsoft.MixedReality.WebRTC.VideoTransceiver.DesiredDirection) property to stop sending (requires renegotiation). This will stop any media data from being sent to the remote peer, even if a track is attached to the transceiver. This can be changed back to send again, but requires another session renegotiation to do so.

  ```cs
  transceiver_.DesiredDirection = Transceiver.Direction.ReceiveOnly;
  peerConnection_.CreateOfferAsync();
  ```

- Similarly for audio methods like `PeerConnection.AddLocalAudioTrackAsync()` and `PeerConnection.RemoveLocalAudioTrack()` were removed in favor for a transceiver-based API.

- Track events for audio and video frames, previously owned by the peer connection, have been moved to the relevant track object.
  - The `PeerConnection.I420ALocalVideoFrameReady` event does not exist anymore; use the [`LocalVideoTrack.I420AVideoFrameReady`](xref:Microsoft.MixedReality.WebRTC.LocalVideoTrack.I420AVideoFrameReady) event instead.


> [!NOTE]
> The transceiver concept is very similar to the same-named concept present in the WebRTC 1.0 standard. There are minor differences around the transceiver naming and automatic track association introduced by MixedReality-WebRTC for simplicity, but for the sake of understanding the media workflow they can be considered the same. For Plan B SDP semantic, MixedReality-WebRTC emulates the same concept over the Plan B semantic, to provide a unified API.

### Standalone media tracks

Previously the `PeerConnection` class was internally owning a local and a remote track for both audio and video. Starting 2.0, those objects are explicit C# classes that the user is creating (local only) and manipulating (both local and remote):

- `PeerConnection.SetLocalVideoTrackEnabled` does not exist anymore; use [`LocalVideoTrack.Enabled`](xref:Microsoft.MixedReality.WebRTC.LocalVideoTrack.Enabled) instead.
- `PeerConnection.IsLocalVideoTrackEnabled` does not exist anymore; use [`LocalVideoTrack.Enabled`](xref:Microsoft.MixedReality.WebRTC.LocalVideoTrack.Enabled) instead.

Note that local track objects are also deriving from [`IDisposable`](xref:System.IDisposable) so must be properly disposed of, like [`PeerConnection`](xref:Microsoft.MixedReality.WebRTC.PeerConnection) and [`DataChannel`](xref:Microsoft.MixedReality.WebRTC.DataChannel).

## Unity project

### Upgrade any project not on 2018.4 LTS

The only version of Unity officially supported is the 2018.4 LTS version. This fixes in particular some bugs with vertical flipping of `SceneVideoSource` captures. Earlier versions of Unity, including 2018.3, are not supported anymore. 2019.x versions generally work, and sometimes provide benefits, but are not officially supported.

### Look out for video shaders vertical flipping

The `YUVFeedShader`, `YUVFeedShaderUnlit`, and `ARGBFeedShader` are now assuming UV coordinates with V going bottom up (OpenGL convention; Unity default) instead of top down (DirectX convention). This corresponds to reverting the check for `UNITY_UV_STARTS_AT_TOP` in the shaders. Users may need to adjust their mesh or manually undo this change to avoid upside-down rendering with existing projects.
