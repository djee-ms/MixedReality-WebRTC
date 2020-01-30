// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using Microsoft.MixedReality.WebRTC.Interop;
using System;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Container for library-wise global settings of MixedReality-WebRTC.
    /// </summary>
    public static class Library
    {
        /// <summary>
        /// Report all objects current alive and tracked by the native implementation.
        /// This is a live report, which generally gets outdated as soon as the function
        /// returned, as new objects are created and others destroyed. Nonetheless this
        /// is may be helpful to diagnose issues with disposing objects.
        /// </summary>
        /// <returns>Returns the number of live objects at the time of the call.</returns>
        public static uint ReportLiveObjects()
        {
            return Utils.LibraryReportLiveObjects();
        }

        /// <summary>
        /// Options of library shutdown.
        /// </summary>
        [Flags]
        public enum ShutdownOptions : uint
        {
            /// <summary>
            /// Do nothing specific.
            /// </summary>
            None = 0,

            /// <summary>
            /// Fail shutdown of some objects are alive.
            /// This is recommended to prevent deadlocks during shutdown.
            /// </summary>
            FailOnLiveObjects = 1,

            /// <summary>
            /// Log all objects still alive, to help debugging.
            /// </summary>
            LogLiveObjects = 2
        }

        /// <summary>
        /// Shutdown the MixedReality-WebRTC library. This must be called once all objects have been
        /// disposed, to shutdown the internal threads and release the global resources, and allow the
        /// library's module to be unloaded.
        /// </summary>
        /// <param name="options">Options for shutdown.</param>
        public static void SetShutdownOptions(ShutdownOptions options)
        {
            Utils.LibrarySetShutdownOptions(options);
        }

        /// <summary>
        /// Forcefully shutdown the MixedReality-WebRTC library. This shall not be used under normal
        /// circumstances, but can be useful e.g. in the Unity editor when a test fails and proper
        /// clean-up is not ensured (in particular, disposing objects), to allow the shared module to
        /// shutdown and terminate its native threads, and be unloaded.
        /// </summary>
        public static void ForceShutdown()
        {
            Utils.LibraryForceShutdown();
        }
    }
}
