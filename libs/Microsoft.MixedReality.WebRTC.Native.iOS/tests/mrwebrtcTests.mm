//
//  mrwebrtcTests.m
//  mrwebrtcTests
//
//  Created by Jérôme HUMBERT on 26/01/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import <XCTest/XCTest.h>
#import <Microsoft_MixedReality_WebRTC/mrwebrtc.h>
#import <Microsoft_MixedReality_WebRTC/audio_session.h>

@interface mrwebrtcTests : XCTestCase

@end

@implementation mrwebrtcTests

- (void)setUp {
    // Ensure the audio session is configured, otherwise audio setup will fail
    XCTAssertTrue([AudioSession configureDefault]);
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testExample {
    PeerConnectionConfiguration config{};
    mrsPeerConnectionInteropHandle dummyHandle = (void*)0x1;
    PeerConnectionHandle handle;
    mrsResult res = mrsPeerConnectionCreate(config, dummyHandle, &handle);
    XCTAssertEqual(mrsResult::kSuccess, res);
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
