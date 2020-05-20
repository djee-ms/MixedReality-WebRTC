// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Concurrent;
using System.Threading.Tasks;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// Base class for media track source components producing some media frames.
    /// </summary>
    /// <seealso cref="AudioTrackSource"/>
    /// <seealso cref="VideoTrackSource"/>
    public abstract class MediaTrackSource : MediaSource
    {
        /// <summary>
        /// Create a new media source of the given <see cref="MediaKind"/>.
        /// </summary>
        /// <param name="mediaKind">The media kind of the source.</param>
        public MediaTrackSource(MediaKind mediaKind) : base(mediaKind)
        {
        }

        protected abstract Task CreateSourceAsync();
        protected abstract void DestroySource();

        protected virtual async void OnEnable()
        {
            await CreateSourceAsync();
        }

        protected virtual void OnDisable()
        {
            DestroySource();
        }
    }
}
