// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Concurrent;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a remote video source added as a video track to an
    /// existing WebRTC peer connection by a remote peer and received locally.
    /// The video track can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Remote Video Source")]
    public class RemoteVideoSource : VideoSource
    {
        /// <summary>
        /// Peer connection this remote video source is extracted from.
        /// </summary>
        [Header("Video track")]
        public PeerConnection PeerConnection;

        /// <summary>
        /// Automatically play the remote video track when it is added.
        /// This is equivalent to manually calling <see cref="Play"/> when the peer connection
        /// is initialized.
        /// </summary>
        /// <seealso cref="Play"/>
        /// <seealso cref="Stop()"/>
        public bool AutoPlayOnAdded = true;

        /// <summary>
        /// Is the video source currently playing?
        /// The concept of _playing_ is described in the <see cref="Play"/> function.
        /// </summary>
        /// <seealso cref="Play"/>
        /// <seealso cref="Stop()"/>
        public bool IsPlaying { get; private set; }

        /// <summary>
        /// Name of the transceiver this component should pair with.
        /// When a remote track associated with a transceiver with that name is added to the peer connection,
        /// this component will automatically pair with that added track.
        /// </summary>
        public string TargetTransceiverName;

        /// <summary>
        /// Remote video track receiving data from the remote peer.
        /// </summary>
        public RemoteVideoTrack Track { get; private set; } = null;

        /// <summary>
        /// Frame queue holding the pending frames enqueued by the video source itself,
        /// which a video renderer needs to read and display.
        /// </summary>
        private VideoFrameQueue<I420AVideoFrameStorage> _frameQueue;

        /// <summary>
        /// Internal queue used to marshal work back to the main Unity thread.
        /// </summary>
        private ConcurrentQueue<Action> _mainThreadWorkQueue = new ConcurrentQueue<Action>();

        /// <summary>
        /// Manually start playback of the remote video feed by registering some listeners
        /// to the peer connection and starting to enqueue video frames as they become ready.
        /// 
        /// Because the WebRTC implementation uses a push model, calling <see cref="Play"/> does
        /// not necessarily start producing frames immediately. Instead, this starts listening for
        /// incoming frames from the remote peer. When a track is actually added by the remote peer
        /// and received locally, the <see cref="VideoSource.VideoStreamStarted"/> event is fired, and soon
        /// after frames will start being available for rendering in the internal frame queue. Note that
        /// this event may be fired before <see cref="Play"/> is called, in which case frames are
        /// produced immediately.
        /// 
        /// If <see cref="AutoPlayOnAdded"/> is <c>true</c> then this is called automatically
        /// as soon as the peer connection is initialized.
        /// </summary>
        /// <remarks>
        /// This is only valid while the peer connection is initialized, that is after the
        /// <see cref="PeerConnection.OnInitialized"/> event was fired.
        /// </remarks>
        /// <seealso cref="Stop()"/>
        /// <seealso cref="IsPlaying"/>
        public void Play()
        {
            if (!IsPlaying)
            {
                IsPlaying = true;
            }
        }

        /// <summary>
        /// Stop playback of the remote video feed and unregister the handler listening to remote
        /// video frames.
        /// 
        /// Note that this is independent of whether or not a remote track is actually present.
        /// In particular this does not fire the <see cref="VideoSource.VideoStreamStopped"/>, which corresponds
        /// to a track being made available to the local peer by the remote peer.
        /// </summary>
        /// <seealso cref="Play()"/>
        /// <seealso cref="IsPlaying"/>
        public void Stop()
        {
            if (IsPlaying)
            {
                IsPlaying = false;
            }
            if (Track != null)
            {
                VideoStreamStopped.Invoke(this);
                Track.Transceiver.LocalTrack = null;
                Track.Dispose();
                Track = null;
            }
        }

        public RemoteVideoSource()
        {
            FrameEncoding = VideoEncoding.I420A;
        }

        public override IVideoFrameQueue GetStats()
        {
            return _frameQueue;
        }

        public override void RegisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady += callback;
            }
        }

        public override void UnregisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady -= callback;
            }
        }

        public override void RegisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady += callback;
            }
        }

        public override void UnregisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady -= callback;
            }
        }

        /// <summary>
        /// Implementation of <a href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.Awake.html">MonoBehaviour.Awake</a>
        /// which registers some handlers with the peer connection to listen to its <see cref="PeerConnection.OnInitialized"/>
        /// and <see cref="PeerConnection.OnShutdown"/> events.
        /// </summary>
        protected void Awake()
        {
            _frameQueue = new VideoFrameQueue<I420AVideoFrameStorage>(5);
            PeerConnection.OnInitialized.AddListener(OnPeerInitialized);
            PeerConnection.OnShutdown.AddListener(OnPeerShutdown);
        }

        /// <summary>
        /// Implementation of <a href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.OnDestroy.html">MonoBehaviour.OnDestroy</a>
        /// which unregisters all listeners from the peer connection.
        /// </summary>
        protected void OnDestroy()
        {
            PeerConnection.OnInitialized.RemoveListener(OnPeerInitialized);
            PeerConnection.OnShutdown.RemoveListener(OnPeerShutdown);
        }

        /// <summary>
        /// Implementation of <a href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.Update.html">MonoBehaviour.Update</a>
        /// to execute from the current Unity main thread any background work enqueued from free-threaded callbacks.
        /// </summary>
        protected void Update()
        {
            // Execute any pending work enqueued by background tasks
            while (_mainThreadWorkQueue.TryDequeue(out Action workload))
            {
                workload();
            }
        }

        /// <summary>
        /// Internal helper callback fired when the peer is initialized, which starts listening for events
        /// on remote tracks added and removed, and optionally starts video playback if the
        /// <see cref="AutoPlayOnAdded"/> property is <c>true</c>.
        /// </summary>
        private void OnPeerInitialized()
        {
            PeerConnection.Peer.VideoTrackAdded += TrackAdded;
            PeerConnection.Peer.VideoTrackRemoved += TrackRemoved;

            if (AutoPlayOnAdded)
            {
                Play();
            }
        }

        /// <summary>
        /// Internal helper callback fired when the peer is shut down, which stops video playback and
        /// unregister all the event listeners from the peer connection about to be destroyed.
        /// </summary>
        private void OnPeerShutdown()
        {
            Stop();
            PeerConnection.Peer.VideoTrackAdded -= TrackAdded;
            PeerConnection.Peer.VideoTrackRemoved -= TrackRemoved;
        }

        /// <summary>
        /// Internal free-threaded helper callback on track added, which enqueues the
        /// <see cref="VideoSource.VideoStreamStarted"/> event to be fired from the main
        /// Unity thread.
        /// </summary>
        private void TrackAdded(RemoteVideoTrack track)
        {
            // Try to bind to the new track by transceiver name, pairing with the remote
            // peer's track with the same transceiver name.
            if ((Track == null) && (track.Transceiver.Name == TargetTransceiverName))
            {
                Track = track;

                // Enqueue invoking the unity event from the main Unity thread, so that listeners
                // can directly access Unity objects from their handler function.
                _mainThreadWorkQueue.Enqueue(() => VideoStreamStarted.Invoke(this));
            }
        }

        /// <summary>
        /// Internal free-threaded helper callback on track removed, which enqueues the
        /// <see cref="VideoSource.VideoStreamStopped"/> event to be fired from the main
        /// Unity thread.
        /// </summary>
        private void TrackRemoved(RemoteVideoTrack track)
        {
            if (Track == track)
            {
                Track = null;

                // Enqueue invoking the unity event from the main Unity thread, so that listeners
                // can directly access Unity objects from their handler function.
                _mainThreadWorkQueue.Enqueue(() => VideoStreamStopped.Invoke(this));
            }
        }
    }
}
