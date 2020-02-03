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
    public abstract class MediaReceiver : MediaSource
    {
        /// <summary>
        /// Automatically play the remote track when it is added.
        /// This is equivalent to manually calling <see cref="Play"/> when the peer connection
        /// is initialized.
        /// </summary>
        /// <seealso cref="Play"/>
        /// <seealso cref="Stop()"/>
        public bool AutoPlayOnAdded = true;

        protected override void OnMediaInitialized()
        {
            if (AutoPlayOnAdded)
            {
                Play();
            }
        }

        protected override void OnMediaShutdown()
        {
            Stop();
        }
    }
}
