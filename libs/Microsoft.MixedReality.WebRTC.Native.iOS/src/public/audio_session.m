// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#import <Foundation/Foundation.h>

#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"
#import "helpers/UIDevice+RTCDevice.h"
#import "base/RTCLogging.h"

#import "audio_session.h"

@interface AudioSession ()
@end

@implementation AudioSession

/**
 Set a default configuration for the audio session. This is necessary before
 trying to initialize a peer connection, to ensure that audio buffers can be
 initialized. Otherwise the init step will assert.
 */
+ (BOOL)configureDefault {
    // Configure the audio session (see ARDMainViewController.m in AppRTCMobile)
    
    // Create a new configuration
    RTCAudioSessionConfiguration *configuration =
    [[RTCAudioSessionConfiguration alloc] init];
    configuration.category = AVAudioSessionCategoryAmbient;
    configuration.categoryOptions = AVAudioSessionCategoryOptionDuckOthers;
    configuration.mode = AVAudioSessionModeDefault;
    
    // Apply the configuration to the audio session singleton
    RTCAudioSession *session = [RTCAudioSession sharedInstance];
    [session lockForConfiguration];
    BOOL hasSucceeded = NO;
    NSError *error = nil;
    if (session.isActive) {
        hasSucceeded = [session setConfiguration:configuration error:&error];
    } else {
        hasSucceeded = [session setConfiguration:configuration
                                          active:YES
                                           error:&error];
    }
    if (!hasSucceeded) {
        RTCLogError(@"Error setting configuration: %@", error.localizedDescription);
    }
    [session unlockForConfiguration];
    return hasSucceeded;
 }

@end
