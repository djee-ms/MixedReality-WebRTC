using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CameraSource : MonoBehaviour
{
    public float Speed = 1f;
    public float HalfAngle = 30f;
    public Transform Target;

    private float _angle = 0f;
    private float _dir = 1f;
    private RenderTexture _renderTex;

    void Start()
    {
        _renderTex = new RenderTexture(320, 240, 0, RenderTextureFormat.BGRA32, RenderTextureReadWrite.Linear);
        GetComponentInChildren<Camera>(includeInactive: true).targetTexture = _renderTex;
    }

    void Update()
    {
        float da = Speed * Time.deltaTime;

        if (_dir > 0f)
        {
            _angle += da;
            if (_angle > HalfAngle)
            {
                _angle = HalfAngle;
                _dir = -1f;
            }
        }
        else
        {
            _angle -= da;
            if (_angle < -HalfAngle)
            {
                _angle = -HalfAngle;
                _dir = 1f;
            }
        }

        Target.localRotation = Quaternion.AngleAxis(_angle, Vector3.forward);
    }
}
