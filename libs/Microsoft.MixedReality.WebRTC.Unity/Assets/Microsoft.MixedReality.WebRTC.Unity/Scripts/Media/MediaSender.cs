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
        /// If left empty, the implementation will generate a GUID name for the track.
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
        [Tooltip("SDP track name")]
        [SdpToken(allowEmpty: true)]
        public string TrackName;

        /// <summary>
        /// Automatically add a track when the peer connection is ready.
        /// </summary>
        public bool AutoAddTrack = true;

        /// <summary>
        /// Automatically start media playback when the component is enabled.
        /// This initializes the media source and starts capture, even if not used by any track yet.
        /// </summary>
        [Tooltip("Automatically start media playback when the component is enabled")]
        public bool AutoPlayOnEnabled = true;

        /// <summary>
        /// Mute the media. For audio, this turns the source to silence. For video, this
        /// produces black frames.
        /// </summary>
        public void Mute()
        {
            MuteImpl(true);
        }

        /// <summary>
        /// Unmute the media and resume normal source playback.
        /// </summary>
        public void Unmute()
        {
            MuteImpl(false);
        }

        protected void OnEnable()
        {
            if (AutoPlayOnEnabled)
            {
                _ = PlayAsync();
            }
        }

        protected void OnDisable()
        {
            Stop();
        }

        /// <summary>
        /// Derived classes implement the mute/unmute action on the track.
        /// </summary>
        /// <param name="mute"><c>true</c> to mute the track, or <c>false</c> to unmute it.</param>
        protected abstract void MuteImpl(bool mute);
    }
}
