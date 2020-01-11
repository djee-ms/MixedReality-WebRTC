// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Diagnostics;
using Microsoft.MixedReality.WebRTC.Interop;
using Microsoft.MixedReality.WebRTC.Tracing;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Audio track sending to the remote peer audio frames originating from
    /// a local track source (local microphone or other audio recording device).
    /// </summary>
    public class LocalAudioTrack : IDisposable
    {
        /// <summary>
        /// Peer connection this audio track is added to, if any.
        /// This is <c>null</c> after the track has been removed from the peer connection.
        /// </summary>
        public PeerConnection PeerConnection { get; private set; }

        /// <summary>
        /// Audio transceiver this track is part of.
        /// </summary>
        public AudioTransceiver Transceiver { get; private set; }

        /// <summary>
        /// Track name as specified during creation. This property is immutable.
        /// </summary>
        public string Name { get; }

        /// <summary>
        /// Enabled status of the track. If enabled, send local audio frames to the remote peer as
        /// expected. If disabled, send only black frames instead.
        /// </summary>
        /// <remarks>
        /// Reading the value of this property after the track has been disposed is valid, and returns
        /// <c>false</c>. Writing to this property after the track has been disposed throws an exception.
        /// </remarks>
        public bool Enabled
        {
            get
            {
                return (LocalAudioTrackInterop.LocalAudioTrack_IsEnabled(_nativeHandle) != 0);
            }
            set
            {
                uint res = LocalAudioTrackInterop.LocalAudioTrack_SetEnabled(_nativeHandle, value ? -1 : 0);
                Utils.ThrowOnErrorCode(res);
            }
        }

        /// <summary>
        /// Event that occurs when a audio frame has been produced by the underlying source and is available.
        /// </summary>
        public event AudioFrameDelegate AudioFrameReady;

        /// <summary>
        /// Handle to the native LocalAudioTrack object.
        /// </summary>
        /// <remarks>
        /// In native land this is a <code>Microsoft::MixedReality::WebRTC::LocalAudioTrackHandle</code>.
        /// </remarks>
        internal LocalAudioTrackHandle _nativeHandle { get; private set; } = new LocalAudioTrackHandle();

        /// <summary>
        /// Handle to self for interop callbacks. This adds a reference to the current object, preventing
        /// it from being garbage-collected.
        /// </summary>
        private IntPtr _selfHandle = IntPtr.Zero;

        /// <summary>
        /// Callback arguments to ensure delegates registered with the native layer don't go out of scope.
        /// </summary>
        private LocalAudioTrackInterop.InteropCallbackArgs _interopCallbackArgs;

        internal LocalAudioTrack(LocalAudioTrackHandle nativeHandle, PeerConnection peer,
            AudioTransceiver transceiver, string trackName)
        {
            _nativeHandle = nativeHandle;
            PeerConnection = peer;
            Transceiver = transceiver;
            transceiver.OnLocalTrackAdded(this);
            Name = trackName;
            RegisterInteropCallbacks();
        }

        private void RegisterInteropCallbacks()
        {
            _interopCallbackArgs = new LocalAudioTrackInterop.InteropCallbackArgs()
            {
                Track = this,
                FrameCallback = LocalAudioTrackInterop.FrameCallback,
            };
            _selfHandle = Utils.MakeWrapperRef(this);
            LocalAudioTrackInterop.LocalAudioTrack_RegisterFrameCallback(
                _nativeHandle, _interopCallbackArgs.FrameCallback, _selfHandle);
        }

        /// <inheritdoc/>
        public void Dispose()
        {
            if (_nativeHandle.IsClosed)
            {
                return;
            }

            // Remove the track from the peer connection, if any
            PeerConnection?.RemoveLocalAudioTrack(this);
            Debug.Assert(PeerConnection == null); // see OnTrackRemoved

            // Unregister interop callbacks
            if (_selfHandle != IntPtr.Zero)
            {
                LocalAudioTrackInterop.LocalAudioTrack_RegisterFrameCallback(_nativeHandle, null, IntPtr.Zero);
                Utils.ReleaseWrapperRef(_selfHandle);
                _selfHandle = IntPtr.Zero;
                _interopCallbackArgs = null;
            }

            // Destroy the native object. This may be delayed if a P/Invoke callback is underway,
            // but will be handled at some point anyway, even if the managed instance is gone.
            _nativeHandle.Dispose();
        }

        internal void OnFrameReady(AudioFrame frame)
        {
            MainEventSource.Log.LocalAudioFrameReady(frame.bitsPerSample, frame.sampleRate, frame.channelCount, frame.sampleCount);
            AudioFrameReady?.Invoke(frame);
        }

        internal void OnTrackAdded(PeerConnection newConnection, AudioTransceiver newTransceiver)
        {
            Debug.Assert(!_nativeHandle.IsClosed);
            Debug.Assert(PeerConnection == null);
            Debug.Assert(Transceiver == null);
            PeerConnection = newConnection;
            Transceiver = newTransceiver;
            newTransceiver.OnLocalTrackAdded(this);
        }

        internal void OnTrackRemoved(PeerConnection previousConnection)
        {
            Debug.Assert(!_nativeHandle.IsClosed);
            Debug.Assert(PeerConnection == previousConnection);
            PeerConnection = null;
            Transceiver.OnLocalTrackRemoved(this);
            Transceiver = null;
        }
    }
}
