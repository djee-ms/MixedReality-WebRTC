// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Concurrent;
using UnityEngine;
using UnityEngine.Events;

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

        /// <summary>
        /// Remote audio track receiving data from the remote peer.
        /// </summary>
        public RemoteAudioTrack Track { get; private set; } = null;

        public void RegisterCallback(AudioFrameDelegate callback) { }
        public void UnregisterCallback(AudioFrameDelegate callback) { }

        protected override void OnPlaybackStarted() { }
        protected override void OnPlaybackStopped()
        {
            if (Track != null)
            {
                AudioStreamStopped.Invoke();
                Track.Transceiver.LocalTrack = null;
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

            // Enqueue invoking the unity event from the main Unity thread, so that listeners
            // can directly access Unity objects from their handler function.
            _mainThreadWorkQueue.Enqueue(() => AudioStreamStarted.Invoke());
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

            // Enqueue invoking the unity event from the main Unity thread, so that listeners
            // can directly access Unity objects from their handler function.
            _mainThreadWorkQueue.Enqueue(() => AudioStreamStopped.Invoke());
        }
    }
}
