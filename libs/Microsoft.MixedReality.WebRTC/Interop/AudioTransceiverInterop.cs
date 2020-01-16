// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Runtime.InteropServices;

namespace Microsoft.MixedReality.WebRTC.Interop
{
    /// <summary>
    /// Handle to a native audio transceiver object.
    /// </summary>
    public sealed class AudioTransceiverHandle : SafeHandle
    {
        /// <summary>
        /// Check if the current handle is invalid, which means it is not referencing
        /// an actual native object. Note that a valid handle only means that the internal
        /// handle references a native object, but does not guarantee that the native
        /// object is still accessible. It is only safe to access the native object if
        /// the handle is not closed, which implies it being valid.
        /// </summary>
        public override bool IsInvalid
        {
            get
            {
                return (handle == IntPtr.Zero);
            }
        }

        /// <summary>
        /// Default constructor for an invalid handle.
        /// </summary>
        public AudioTransceiverHandle() : base(IntPtr.Zero, ownsHandle: true)
        {
        }

        /// <summary>
        /// Constructor for a valid handle referencing the given native object.
        /// </summary>
        /// <param name="handle">The valid internal handle to the native object.</param>
        public AudioTransceiverHandle(IntPtr handle) : base(IntPtr.Zero, ownsHandle: true)
        {
            SetHandle(handle);
        }

        /// <summary>
        /// Release the native object while the handle is being closed.
        /// </summary>
        /// <returns>Return <c>true</c> if the native object was successfully released.</returns>
        protected override bool ReleaseHandle()
        {
            AudioTransceiverInterop.AudioTransceiver_RemoveRef(handle);
            return true;
        }
    }

    internal class AudioTransceiverInterop
    {
        #region Native functions

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverAddRef")]
        public static unsafe extern void AudioTransceiver_AddRef(AudioTransceiverHandle handle);

        // Note - This is used during SafeHandle.ReleaseHandle(), so cannot use AudioTransceiverHandle
        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverRemoveRef")]
        public static unsafe extern void AudioTransceiver_RemoveRef(IntPtr handle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverRegisterStateUpdatedCallback")]
        public static unsafe extern uint AudioTransceiver_RegisterStateUpdatedCallback(AudioTransceiverHandle handle,
            StateUpdatedDelegate callback);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverSetDirection")]
        public static unsafe extern uint AudioTransceiver_SetDirection(AudioTransceiverHandle handle,
            Transceiver.Direction newDirection);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverSetLocalTrack")]
        public static unsafe extern uint AudioTransceiver_SetLocalTrack(AudioTransceiverHandle handle,
            LocalAudioTrackHandle trackHandle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverGetLocalTrack")]
        public static unsafe extern uint AudioTransceiver_GetLocalTrack(AudioTransceiverHandle handle,
            ref LocalAudioTrackHandle trackHandle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsAudioTransceiverGetRemoteTrack")]
        public static unsafe extern uint AudioTransceiver_GetRemoteTrack(AudioTransceiverHandle handle,
            ref RemoteAudioTrackHandle trackHandle);

        #endregion


        #region Marshaling data structures

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct CreateConfig
        {
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        internal ref struct InitConfig
        {
            /// <summary>
            /// Handle to the audio transceiver wrapper.
            /// </summary>
            public IntPtr transceiverHandle;

            public InitConfig(AudioTransceiver transceiver, AudioTransceiverInitSettings settings)
            {
                transceiverHandle = Utils.MakeWrapperRef(transceiver);
            }
        }

        #endregion


        #region Native callbacks

        public static readonly StateUpdatedDelegate StateUpdatedCallback = AudioTransceiverStateUpdatedCallback;

        [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        public delegate IntPtr CreateObjectDelegate(IntPtr peer, in CreateConfig config);

        [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        public delegate void StateUpdatedDelegate(IntPtr transceiver,
            Transceiver.Direction negotiatedDirection, Transceiver.Direction desiredDirection);

        [MonoPInvokeCallback(typeof(CreateObjectDelegate))]
        public static IntPtr AudioTransceiverCreateObjectCallback(IntPtr peer, in CreateConfig config)
        {
            var peerWrapper = Utils.ToWrapper<PeerConnection>(peer);
            var audioTranceiverWrapper = CreateWrapper(peerWrapper, in config);
            return Utils.MakeWrapperRef(audioTranceiverWrapper);
        }

        [MonoPInvokeCallback(typeof(StateUpdatedDelegate))]
        private static void AudioTransceiverStateUpdatedCallback(IntPtr transceiver,
            Transceiver.Direction negotiatedDirection, Transceiver.Direction desiredDirection)
        {
            var audioTranceiverWrapper = Utils.ToWrapper<AudioTransceiver>(transceiver);
            audioTranceiverWrapper.OnStateUpdated(negotiatedDirection, desiredDirection);
        }

        #endregion


        #region Utilities

        public static AudioTransceiver CreateWrapper(PeerConnection parent, in CreateConfig config)
        {
            return new AudioTransceiver(parent);
        }

        public static void RegisterCallbacks(AudioTransceiverHandle handle)
        {
            AudioTransceiver_RegisterStateUpdatedCallback(handle, StateUpdatedCallback);
        }

        public static void UnregisterCallbacks(AudioTransceiverHandle handle)
        {
            AudioTransceiver_RegisterStateUpdatedCallback(handle, null);
        }

        #endregion
    }
}
