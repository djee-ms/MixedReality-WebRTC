// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "data_channel.h"
#include "external_audio_track_source_interop.h"
#include "interop_api.h"
#include "local_audio_track_interop.h"
#include "remote_audio_track_interop.h"
#include "transceiver_interop.h"

#include "test_utils.h"

namespace {

class ExternalAudioTrackSourceTests
    : public TestUtils::TestBase,
      public testing::WithParamInterface<mrsSdpSemantic> {};

}  // namespace

#if !defined(MRSW_EXCLUDE_DEVICE_TESTS)

namespace {

constexpr float kTwoPi = 6.283185307179586476925286766559f;

/// Frame generator producing a sine wave at a given frequency.
struct SineGenerator {
  SineGenerator(float frequency_hz = 400.0f,
                int sampling_frequency_hz = 48000,
                int channel_count = 1)
      : frequency_hz_(frequency_hz),
        sampling_frequency_hz_(sampling_frequency_hz),
        channel_count_(channel_count) {}

  /// Trampoline for interop callback.
  static mrsResult MRS_CALL
  GenerateAudioTestFrame(void* user_data,
                         mrsExternalAudioTrackSourceHandle source_handle,
                         uint32_t request_id,
                         int64_t timestamp_ms) {
    SineGenerator* const self = static_cast<SineGenerator*>(user_data);
    return self->GenerateAudioTestFrameImpl(source_handle, request_id,
                                            timestamp_ms);
  }

  /// Generate a test frame with a sine.
  mrsResult GenerateAudioTestFrameImpl(
      mrsExternalAudioTrackSourceHandle source_handle,
      uint32_t request_id,
      int64_t timestamp_ms) {
    memset(buffer_, 0, 480 * 2 * 1);
    int16_t* ptr = buffer_;
    const float dt = 1.0f / sampling_frequency_hz_;
    // Implementation only supports exactly 10ms
    // FIXME - This should be reflected in the API
    // See AudioCodingModuleImpl::Add10MsDataInternal()
    const int num_samples_per_channel = sampling_frequency_hz_ / 100;
    assert(num_samples_per_channel <= 480);
    float t = current_time_;
    const float t0 = t;
    const float ratio = kTwoPi * frequency_hz_;
    int16_t val = 0;
    if (channel_count_ == 1) {
      for (int i = 0; i < num_samples_per_channel; ++i) {
        val = (int16_t)(sin(ratio * t) * 32767.0f);
        *ptr++ = val;
        t += dt;
      }
    } else if (channel_count_ == 2) {
      for (int i = 0; i < num_samples_per_channel; ++i) {
        val = (int16_t)(sin(ratio * t) * 32767.0f);
        *ptr++ = val;  // L
        *ptr++ = val;  // R
        t += dt;
      }
    }
    current_time_ = t;
    char buffer[1024];
    snprintf(buffer, 1024, "Generate audio data : %f -> %f (last = %d s16)\n",
             t0, current_time_, (int)val);
    OutputDebugStringA(buffer);
    mrsAudioFrame frame_view{};
    frame_view.data_ = buffer_;
    frame_view.bits_per_sample_ = 16;
    frame_view.sampling_rate_hz_ = sampling_frequency_hz_;
    frame_view.channel_count_ = channel_count_;
    frame_view.sample_count_ = num_samples_per_channel;
    return mrsExternalAudioTrackSourceCompleteFrameRequest(
        source_handle, request_id, timestamp_ms, &frame_view);
  }

  /// Helper class to validate a received audio frame generated by a
  /// SineGenerator.
  class Validator {
   public:
    Validator(float frequency_hz = 400.0f) : frequency_hz_(frequency_hz) {}

    void validateFrame(const mrsAudioFrame& frame) {
      ASSERT_NE(nullptr, frame.data_);
      ASSERT_EQ(16u, frame.bits_per_sample_);
      ASSERT_EQ(1u, frame.channel_count_);
      // Note - sampling_rate_hz maybe be different from the one used in
      // GenerateAudioTestFrame(), this depends on the mixer.
      double err = 0.0;
      const int16_t* ptr = (const int16_t*)frame.data_;
      const float dt = 1.0f / frame.sampling_rate_hz_;
      float t = current_time_;
      const float ratio = kTwoPi * frequency_hz_;
      for (uint32_t i = 0; i < frame.sample_count_; ++i) {
        int16_t ref = (int16_t)(sin(ratio * t) * 32767.0f);
        t += dt;
        err += abs(*ptr++ - ref);
      }
      current_time_ = t;
      char buffer[1024];
      snprintf(buffer, 1024, "Validate audio data : frame=%d err=%f\n",
               frame_count_++, err);
      OutputDebugStringA(buffer);
      // ASSERT_LE(std::fabs(err), 1.0);
    }

    uint32_t getFrameCount() const { return frame_count_; }

   protected:
    float current_time_{0.0f};
    float frequency_hz_{400.0f};
    uint32_t frame_count_{0};
  };

  /// Get a validator helper for the current SineGenerator instance.
  Validator getValidator() const { return Validator(frequency_hz_); }

 protected:
  float current_time_{0.0f};
  float frequency_hz_{400.0f};
  int sampling_frequency_hz_{16000};
  int channel_count_{1};
  // Static buffer for 480 samples stereo (10ms @ 48kHz)
  int16_t buffer_[480 * 2];
};

// PeerConnectionAudioTrackAddedCallback
using AudioTrackAddedCallback =
    InteropCallback<const mrsRemoteAudioTrackAddedInfo*>;

// mrsAudioFrameCallback
using AudioFrameCallback = InteropCallback<const mrsAudioFrame&>;

}  // namespace

INSTANTIATE_TEST_CASE_P(,
                        ExternalAudioTrackSourceTests,
                        testing::ValuesIn(TestUtils::TestSemantics),
                        TestUtils::SdpSemanticToString);

TEST_P(ExternalAudioTrackSourceTests, Simple) {
  mrsPeerConnectionConfiguration pc_config{};
  pc_config.sdp_semantic = GetParam();
  LocalPeerPairRaii pair(pc_config);

  // Grab the handle of the remote track from the remote peer (#2) via the
  // AudioTrackAdded callback.
  mrsRemoteAudioTrackHandle track_handle2{};
  mrsTransceiverHandle transceiver_handle2{};
  Event track_added2_ev;
  AudioTrackAddedCallback track_added2_cb =
      [&track_handle2, &transceiver_handle2,
       &track_added2_ev](const mrsRemoteAudioTrackAddedInfo* info) {
        track_handle2 = info->track_handle;
        transceiver_handle2 = info->audio_transceiver_handle;
        track_added2_ev.Set();
      };
  mrsPeerConnectionRegisterAudioTrackAddedCallback(pair.pc2(),
                                                   CB(track_added2_cb));

  // Create the external source for the local audio track of the local peer (#1)
  SineGenerator generator(600.0f, 48000, 2);
  mrsExternalAudioTrackSourceHandle source_handle1 = nullptr;
  ASSERT_EQ(mrsResult::kSuccess, mrsExternalAudioTrackSourceCreateFromCallback(
                                     &SineGenerator::GenerateAudioTestFrame,
                                     &generator, &source_handle1));
  ASSERT_NE(nullptr, source_handle1);
  mrsExternalAudioTrackSourceFinishCreation(source_handle1);

  // Create the local track itself for #1
  mrsLocalAudioTrackHandle track_handle1{};
  {
    mrsLocalAudioTrackInitSettings settings{};
    settings.track_name = "sine_track";
    ASSERT_EQ(mrsResult::kSuccess,
              mrsLocalAudioTrackCreateFromSource(&settings, source_handle1,
                                                 &track_handle1));
    ASSERT_NE(nullptr, track_handle1);
    ASSERT_NE(mrsBool::kFalse, mrsLocalAudioTrackIsEnabled(track_handle1));
  }

  // Create the audio transceiver #1
  mrsTransceiverHandle transceiver_handle1{};
  {
    mrsTransceiverInitConfig transceiver_config{};
    transceiver_config.name = "transceiver_1";
    transceiver_config.media_kind = mrsMediaKind::kAudio;
    transceiver_config.desired_direction = mrsTransceiverDirection::kSendOnly;
    ASSERT_EQ(mrsResult::kSuccess,
              mrsPeerConnectionAddTransceiver(pair.pc1(), &transceiver_config,
                                              &transceiver_handle1));
    ASSERT_NE(nullptr, transceiver_handle1);
  }

  // Add the track #1 to the transceiver #1
  ASSERT_EQ(mrsResult::kSuccess, mrsTransceiverSetLocalAudioTrack(
                                     transceiver_handle1, track_handle1));

  // Connect #1 and #2
  pair.ConnectAndWait();

  // Wait for remote track to be added on #2
  ASSERT_TRUE(track_added2_ev.WaitFor(5s));
  ASSERT_NE(nullptr, track_handle2);
  ASSERT_NE(nullptr, transceiver_handle2);

  // Register a frame callback for the remote audio of #2
  auto validator = generator.getValidator();
  AudioFrameCallback argb_cb = [&validator](const mrsAudioFrame& frame) {
    validator.validateFrame(frame);
  };
  mrsRemoteAudioTrackRegisterFrameCallback(track_handle2, CB(argb_cb));

  // Wait 3 seconds and check the frame callback is called
  Event ev;
  ev.WaitFor(3s);
  ASSERT_LT(30u, validator.getFrameCount()) << "Expected at least 10 FPS";

  // Clean-up
  mrsRemoteAudioTrackRegisterFrameCallback(track_handle2, nullptr, nullptr);
  mrsRefCountedObjectRemoveRef(track_handle1);
  mrsExternalAudioTrackSourceShutdown(source_handle1);
  mrsRefCountedObjectRemoveRef(source_handle1);
}

#endif  // MRSW_EXCLUDE_DEVICE_TESTS
