// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Concurrent;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a media receiver added as a media track to an
    /// existing WebRTC peer connection by a remote peer and received locally.
    /// The media track can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    public abstract class MediaSource : MonoBehaviour
    {
        /// <summary>
        /// Peer connection this media sender is extracting its track from.
        /// </summary>
        [Header("Connection")]
        public PeerConnection PeerConnection;

        /// <summary>
        /// Is the source currently playing?
        /// The concept of _playing_ is described in the <see cref="Play"/> function.
        /// </summary>
        /// <seealso cref="Play"/>
        /// <seealso cref="Stop()"/>
        public bool IsPlaying { get; private set; }

        /// <summary>
        /// Internal queue used to marshal work back to the main Unity thread.
        /// </summary>
        protected ConcurrentQueue<Action> _mainThreadWorkQueue = new ConcurrentQueue<Action>();

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
                OnPlaybackStarted();
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
                OnPlaybackStopped();
                IsPlaying = false;
            }
        }

        private enum MediaState
        {
            Idle,
            Initializing,
            Ready,
            ShuttingDown
        }

        private MediaState _mediaState = MediaState.Idle;
        private object _mediaStateLock = new object();

        protected virtual void OnPlaybackStarted() { }
        protected virtual void OnPlaybackStopped() { }

        protected abstract void OnMediaInitialized();
        protected abstract void OnMediaShutdown();

        /// <summary>
        /// Implementation of <a href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.Awake.html">MonoBehaviour.Awake</a>
        /// which registers some handlers with the peer connection to listen to its <see cref="PeerConnection.OnInitialized"/>
        /// and <see cref="PeerConnection.OnShutdown"/> events.
        /// </summary>
        protected void Awake()
        {
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

        protected void OnEnable()
        {
            var nativePeer = PeerConnection.Peer;
            if ((nativePeer != null) && (nativePeer.Initialized))
            {
                lock (_mediaStateLock)
                {
                    if (_mediaState != MediaState.Idle)
                    {
                        return;
                    }
                    _mediaState = MediaState.Initializing;
                }
                OnMediaInitialized();
                lock (_mediaStateLock)
                {
                    Debug.Assert(_mediaState == MediaState.Initializing);
                    _mediaState = MediaState.Ready;
                }
            }
        }

        protected void OnDisable()
        {
            lock (_mediaStateLock)
            {
                if (_mediaState != MediaState.Ready)
                {
                    return;
                }
                _mediaState = MediaState.ShuttingDown;
            }
            OnMediaShutdown();
            lock (_mediaStateLock)
            {
                Debug.Assert(_mediaState == MediaState.ShuttingDown);
                _mediaState = MediaState.Idle;
            }
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
            if (enabled)
            {
                OnMediaInitialized();
            }
        }

        /// <summary>
        /// Internal helper callback fired when the peer is shut down, which stops video playback and
        /// unregister all the event listeners from the peer connection about to be destroyed.
        /// </summary>
        private void OnPeerShutdown()
        {
            OnMediaShutdown();
        }
    }
}
