using Microsoft.MixedReality.WebRTC;
using Microsoft.MixedReality.WebRTC.Unity;
using UnityEngine;

public class VideoTrackSourceStats : MonoBehaviour
{
    public SceneVideoSource Source;
    public TextMesh Text;

    private ExternalVideoTrackSource.ExternalVideoTrackSourceStats _stats = new ExternalVideoTrackSource.ExternalVideoTrackSourceStats();

    void Update()
    {
        var src = (ExternalVideoTrackSource)(Source.Source);
        src.GetStats(ref _stats);
        Text.text = $"{_stats.NumFramesProduced} | {_stats.NumFramesSkipped} | {_stats.AvgFramerate} fps";
    }
}
