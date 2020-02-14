// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System.Threading.Tasks;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a remote audio source added as an audio track to an
    /// existing WebRTC peer connection by a remote peer and received locally.
    /// The audio track can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Audio Receiver")]
    public class AudioReceiver : MediaReceiver, IAudioSource
    {
        public AudioStreamStartedEvent AudioStreamStarted = new AudioStreamStartedEvent();
        public AudioStreamStoppedEvent AudioStreamStopped = new AudioStreamStoppedEvent();

        public AudioStreamStartedEvent GetAudioStreamStarted() { return AudioStreamStarted; }
        public AudioStreamStoppedEvent GetAudioStreamStopped() { return AudioStreamStopped; }

        /// <summary>
        /// Audio transceiver this receiver is paired with.
        /// This is <c>null</c> until a remote description is applied which pairs the receiver
        /// with the remote track of the transceiver, or until the peer connection associated
        /// with this receiver creates the audio receiver right before creating an SDP offer.
        /// </summary>
        public AudioTransceiver Transceiver { get; private set; }

        /// <summary>
        /// Remote audio track receiving data from the remote peer.
        /// This is <c>null</c> until a remote description is applied which pairs the receiver
        /// with the remote track of the transceiver.
        /// </summary>
        public RemoteAudioTrack Track { get; private set; }

        /// <summary>
        /// Register a frame callback to listen to incoming audio data receiving through this
        /// audio receiver from the remote peer.
        /// The callback can only be registered once the <see cref="Track"/> is valid, that is
        /// once the <see cref="AudioStreamStarted"/> event was triggered.
        /// </summary>
        /// <param name="callback">The new frame callback to register.</param>
        /// <remarks>
        /// A typical application might use this callback to display some feedback of a local webcam recording.
        /// 
        /// Note that registering a callback does not influence the video capture and sending to the
        /// remote peer, which occurs whether or not a callback is registered.
        /// </remarks>
        public void RegisterCallback(AudioFrameDelegate callback) { }

        /// <summary>
        /// Unregister an existing frame callback registered with <see cref="RegisterCallback(AudioFrameDelegate)"/>.
        /// </summary>
        /// <param name="callback">The frame callback to unregister.</param>
        public void UnregisterCallback(AudioFrameDelegate callback) { }

        /// <summary>
        /// Internal callback invoked when the audio receiver is attached to a transceiver created
        /// just before the peer connection creates an SDP offer.
        /// </summary>
        /// <param name="audioTransceiver">The audio transceiver this receiver is attached with.</param>
        /// <remarks>
        /// At this time the transceiver does not yet contain a remote track. The remote track will be
        /// created when receiving an answer from the remote peer, if it agreed to send media data through
        /// that transceiver, and <see cref="OnPaired"/> will be invoked at that time.
        /// </remarks>
        internal void AttachToTransceiver(AudioTransceiver audioTransceiver)
        {
            Debug.Assert(Transceiver == null);
            Transceiver = audioTransceiver;
        }

        protected override Task DoStartMediaPlaybackAsync()
        {
            return Task.CompletedTask;
        }

        protected override void DoStopMediaPlayback()
        {
            if (Track != null)
            {
                AudioStreamStopped.Invoke();

                // Track may not be added to any transceiver (e.g. no connection)
                if (Track.Transceiver != null)
                {
                    Track.Transceiver.LocalTrack = null;
                }

                // Remote tracks are owned by the peer connection (not disposable)
                Track = null;
            }
        }

        /// <summary>
        /// Free-threaded callback invoked by the owning peer connection when a track is paired
        /// with this receiver, which enqueues the <see cref="AudioSource.AudioStreamStarted"/>
        /// event to be fired from the main Unity thread.
        /// </summary>
        internal void OnPaired(RemoteAudioTrack track)
        {
            Debug.Assert(Track == null);
            Track = track;

            if (AutoPlayOnPaired)
            {
                PlayAsync().ContinueWith(_ => //IsPlaying = true;
                {
                    // Enqueue invoking the unity event from the main Unity thread, so that listeners
                    // can directly access Unity objects from their handler function.
                    _mainThreadWorkQueue.Enqueue(() => AudioStreamStarted.Invoke());
                });
            }
        }

        /// <summary>
        /// Free-threaded callback invoked by the owning peer connection when a track is unpaired
        /// from this receiver, which enqueues the <see cref="AudioSource.AudioStreamStopped"/>
        /// event to be fired from the main Unity thread.
        /// </summary>
        internal void OnUnpaired(RemoteAudioTrack track)
        {
            Debug.Assert(Track == track);
            Track = null;

            //IsPlaying = false;

            // Enqueue invoking the unity event from the main Unity thread, so that listeners
            // can directly access Unity objects from their handler function.
            _mainThreadWorkQueue.Enqueue(() => AudioStreamStopped.Invoke());
        }
    }
}
