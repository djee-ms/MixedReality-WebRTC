// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace Microsoft.MixedReality.WebRTC.Unity.Editor
{
    [CustomEditor(typeof(PeerConnection))]
    [CanEditMultipleObjects]
    public class PeerConnectionEditor : UnityEditor.Editor
    {
        const float kCenterMargin = 12;
        const float kIconSpacing = 25;
        const float kVerticalSpacing = 0;
        const float kLineHeight = 22;

        SerializedProperty autoInitOnStart;
        SerializedProperty autoLogErrors;

        SerializedProperty iceServers;
        SerializedProperty iceUsername;
        SerializedProperty iceCredential;

        SerializedProperty onInitialized;
        SerializedProperty onShutdown;
        SerializedProperty onError;

        ReorderableList transceiverList_;
        SerializedProperty transceivers_;

        void OnEnable()
        {
            autoInitOnStart = serializedObject.FindProperty("AutoInitializeOnStart");
            autoLogErrors = serializedObject.FindProperty("AutoLogErrorsToUnityConsole");

            iceServers = serializedObject.FindProperty("IceServers");
            iceUsername = serializedObject.FindProperty("IceUsername");
            iceCredential = serializedObject.FindProperty("IceCredential");

            onInitialized = serializedObject.FindProperty("OnInitialized");
            onShutdown = serializedObject.FindProperty("OnShutdown");
            onError = serializedObject.FindProperty("OnError");

            transceivers_ = serializedObject.FindProperty("_transceivers");
            transceiverList_ = new ReorderableList(serializedObject, transceivers_, true, true, true, true);
            transceiverList_.elementHeight = 2 * kLineHeight + kVerticalSpacing;
            transceiverList_.drawHeaderCallback = DrawHeader;
            transceiverList_.drawElementCallback =
                (Rect rect, int index, bool isActive, bool isFocused) =>
                {
                    var element = transceiverList_.serializedProperty.GetArrayElementAtIndex(index);

                    float h = kLineHeight;
                    float y0 = rect.y + 2;
                    float y1 = y0 + h + kVerticalSpacing;

                    //EditorGUI.PropertyField(new Rect(rect.x, rect.y, 100, EditorGUIUtility.singleLineHeight), element.FindPropertyRelative("Type"), GUIContent.none);
                    //EditorGUI.DrawTextureTransparent(new Rect(rect.x, rect.y, 16, 16), EditorGUIUtility.IconContent(""));
                    MediaKind kind = (MediaKind)element.FindPropertyRelative("Kind").intValue;
                    System.Type senderType, receiverType;
                    if (kind == MediaKind.Audio)
                    {
                        senderType = typeof(AudioSender);
                        receiverType = typeof(AudioReceiver);
                        EditorGUI.LabelField(new Rect(rect.x, rect.y, 22, 22), EditorGUIUtility.IconContent("AudioSource Icon")); // AudioSource Icon")); // "Profiler.Video"
                    }
                    else
                    {
                        senderType = typeof(VideoSender);
                        receiverType = typeof(VideoReceiver);
                        EditorGUI.LabelField(new Rect(rect.x, rect.y, 22, 22), EditorGUIUtility.IconContent("Profiler.Video")); // AudioSource Icon")); // "Profiler.Video"
                    }
                    EditorGUI.LabelField(new Rect(rect.x, y1, 22, 22), $"{index}");

                    rect.x += kIconSpacing;
                    rect.width -= kIconSpacing;
                    //EditorGUI.PropertyField(rect, element.FindPropertyRelative("Type"), GUIContent.none);
                    //EditorGUI.PropertyField(
                    //    new Rect(rect.x, rect.y, (rect.width - kCenterMargin) / 2, EditorGUIUtility.singleLineHeight),
                    //    element.FindPropertyRelative("Sender"), GUIContent.none);
                    //EditorGUI.PropertyField(
                    //    new Rect(rect.x + (rect.width - kCenterMargin) / 2 + kCenterMargin, rect.y, (rect.width - kCenterMargin) / 2, EditorGUIUtility.singleLineHeight),
                    //    element.FindPropertyRelative("Receiver"), GUIContent.none);

                    EditorGUI.LabelField(new Rect(rect.x, y0, 22, 22), EditorGUIUtility.IconContent("tab_next"));
                    EditorGUI.LabelField(new Rect(rect.x, y1, 22, 22), EditorGUIUtility.IconContent("tab_prev"));

                    rect.x += 22;
                    rect.width -= 22;

                    float fieldWidth = rect.width; // (rect.width - kCenterMargin) / 2;
                    {

                        var p = element.FindPropertyRelative("Sender");
                        Object obj = p.objectReferenceValue;
                        obj = EditorGUI.ObjectField(
                            new Rect(rect.x, y0, fieldWidth, EditorGUIUtility.singleLineHeight),
                            obj, senderType, true);
                        p.objectReferenceValue = obj;
                    }
                    {
                        var p = element.FindPropertyRelative("Receiver");
                        Object obj = p.objectReferenceValue;
                        obj = EditorGUI.ObjectField(
                            new Rect(rect.x, y1, fieldWidth, EditorGUIUtility.singleLineHeight),
                            obj, receiverType, true);
                        p.objectReferenceValue = obj;
                    }
                };
            transceiverList_.drawNoneElementCallback = (Rect rect) =>
            {
                GUIStyle style = new GUIStyle(EditorStyles.label);
                style.alignment = TextAnchor.MiddleCenter;
                EditorGUI.LabelField(rect, "(empty)", style);
            };
            transceiverList_.displayAdd = false;
            //transceiverList_.drawFooterCallback = (Rect rect) =>
            //{
            //    ReorderableList.defaultBehaviours.DrawFooter(rect, transceiverList_);
            //};
            //transceiverList_.onChangedCallback = (ReorderableList list) =>
            //{

            //};
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.LabelField("Behavior", EditorStyles.boldLabel);
            autoInitOnStart.boolValue = EditorGUILayout.ToggleLeft(autoInitOnStart.displayName, autoInitOnStart.boolValue);
            autoLogErrors.boolValue = EditorGUILayout.ToggleLeft(autoLogErrors.displayName, autoLogErrors.boolValue);

            EditorGUILayout.PropertyField(iceServers, true);
            EditorGUILayout.PropertyField(iceUsername);
            EditorGUILayout.PropertyField(iceCredential);

            EditorGUILayout.Space();

            EditorGUILayout.LabelField("Transceivers", EditorStyles.boldLabel);
            transceiverList_.DoLayoutList();
            using (var _ = new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("+ Audio"))
                {
                    ((PeerConnection)serializedObject.targetObject).AddTransceiver(MediaKind.Audio);
                }
                if (GUILayout.Button("+ Video"))
                {
                    ((PeerConnection)serializedObject.targetObject).AddTransceiver(MediaKind.Video);
                }
            }

            EditorGUILayout.PropertyField(onInitialized);
            EditorGUILayout.PropertyField(onShutdown);
            EditorGUILayout.PropertyField(onError);

            serializedObject.ApplyModifiedProperties();
        }

        private void DrawHeader(Rect rect)
        {
            const float kLeftMargin = 12 + kIconSpacing;
            rect.width -= kLeftMargin;
            rect.x += kLeftMargin;
            rect.width = (rect.width - kCenterMargin) / 2;
            EditorGUI.LabelField(rect, "Sender");
            rect.x += rect.width + kCenterMargin;
            EditorGUI.LabelField(rect, "Receiver");
        }
    }

}
