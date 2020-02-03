// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local media source added as a media track to an
    /// existing WebRTC peer connection and sent to the remote peer. The track can
    /// optionally be rendered locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    public abstract class MediaSender : MediaSource
    {
        /// <summary>
        /// Name of the track.
        /// </summary>
        /// <remarks>
        /// This must comply with the 'msid' attribute rules as defined in
        /// https://tools.ietf.org/html/draft-ietf-mmusic-msid-05#section-2, which in
        /// particular constraints the set of allows characters to those allowed for a
        /// 'token' element as specified in https://tools.ietf.org/html/rfc4566#page-43:
        /// - Symbols [!#$%'*+-.^_`{|}~] and ampersand &amp;
        /// - Alphanumerical [A-Za-z0-9]
        /// </remarks>
        /// <seealso xref="SdpTokenAttribute.ValidateSdpTokenName"/>
        [Tooltip("SDP track name.")]
        [SdpToken(allowEmpty: true)]
        public string TrackName;

        /// <summary>
        /// Automatically add a track when the peer connection is ready.
        /// </summary>
        public bool AutoAddTrack = true;

        public void Mute()
        {
            MuteImpl(true);
        }

        public void Unmute()
        {
            MuteImpl(false);
        }

        protected abstract void MuteImpl(bool mute);

        protected override void OnMediaInitialized()
        {
            if (AutoAddTrack)
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
