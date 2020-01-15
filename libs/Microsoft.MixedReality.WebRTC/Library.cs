// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using Microsoft.MixedReality.WebRTC.Interop;
using System;

namespace Microsoft.MixedReality.WebRTC
{
    public static class Library
    {
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
    }
}
