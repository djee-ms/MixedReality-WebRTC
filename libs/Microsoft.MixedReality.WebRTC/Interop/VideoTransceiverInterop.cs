// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Runtime.InteropServices;

namespace Microsoft.MixedReality.WebRTC.Interop
{
    /// <summary>
    /// Handle to a native video transceiver object.
    /// </summary>
    public sealed class VideoTransceiverHandle : SafeHandle
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
        public VideoTransceiverHandle() : base(IntPtr.Zero, ownsHandle: true)
        {
        }

        /// <summary>
        /// Constructor for a valid handle referencing the given native object.
        /// </summary>
        /// <param name="handle">The valid internal handle to the native object.</param>
        public VideoTransceiverHandle(IntPtr handle) : base(IntPtr.Zero, ownsHandle: true)
        {
            SetHandle(handle);
        }

        /// <summary>
        /// Release the native object while the handle is being closed.
        /// </summary>
        /// <returns>Return <c>true</c> if the native object was successfully released.</returns>
        protected override bool ReleaseHandle()
        {
            VideoTransceiverInterop.VideoTransceiver_RemoveRef(handle);
            return true;
        }
    }

    internal class VideoTransceiverInterop
    {
        #region Native functions

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsVideoTransceiverAddRef")]
        public static unsafe extern void VideoTransceiver_AddRef(VideoTransceiverHandle handle);

        // Note - This is used during SafeHandle.ReleaseHandle(), so cannot use VideoTransceiverHandle
        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsVideoTransceiverRemoveRef")]
        public static unsafe extern void VideoTransceiver_RemoveRef(IntPtr handle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsVideoTransceiverSetLocalTrack")]
        public static unsafe extern uint VideoTransceiver_SetLocalTrack(VideoTransceiverHandle handle,
            LocalVideoTrackHandle trackHandle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsVideoTransceiverGetLocalTrack")]
        public static unsafe extern uint VideoTransceiver_GetLocalTrack(VideoTransceiverHandle handle,
            out LocalVideoTrackHandle trackHandle);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsVideoTransceiverGetRemoteTrack")]
        public static unsafe extern uint VideoTransceiver_GetRemoteTrack(VideoTransceiverHandle handle,
            out RemoteVideoTrackHandle trackHandle);

        #endregion
    }
}
