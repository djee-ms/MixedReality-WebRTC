Add standalone media track objects in both the C++ and C# libraries. This enables supporting multiple audio and video tracks per peer connection.

For local tracks, the `PeerConnection` interface is updated such that `AddLocalAudioTrackAsync()` and `AddLocalVideoTrackAsync()` now return a track object, allowing the user to manipulate it directly without the need to go through the associated peer connection. For remote tracks, the `TrackAdded` and `TrackRemoved` events are now invoked with the remote track object as argument. The C# `PeerConnection` object also now keeps a list of all the tracks attached to it, which can therefore be enumerated.

Because tracks are standalone objects, methods like `PeerConnection.SetLocalAudioTrackEnabled()` are replaced with a property `LocalAudioTrack.Enabled`, providing a more familiar pattern to C# developers.

One important change required for remote tracks to be paired by name is removing the legacy options `RTCOfferAnswerOptions::offer_to_receive_video = true` and `RTCOfferAnswerOptions::offer_to_receive_audio = true` from `CreateOffer()` and `CreateAnswer()`. This change (see #75) has larger implications that it looks when using Unified Plan (default), as 

A new Unity demo `MultiTrackDemo` shows how to use multiple tracks. The 

Bug: #152