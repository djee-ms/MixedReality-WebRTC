//
//  SessionModel.m
//  mr-webrtc-testapp
//
//  Created by Jérôme HUMBERT on 15/06/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import "SessionModel.h"

#include <Microsoft_MixedReality_WebRTC/interop_api.h>
#include <Microsoft_MixedReality_WebRTC/peer_connection_interop.h>

enum class SdpMessageType : int {
  kSdpOffer = 1,
  kSdpAnswer = 2,
  kSdpIceCandidate = 3
};

struct SdpMessage {
  SdpMessageType messageType;
  NSString *data;
};

@interface SessionModel()
{
  dispatch_queue_t _pollQueue;
  NSURL *_getURL;
  NSURL *_postURL;
}
@end

@implementation SessionModel

- (id)init {
  if (self = [super init]) {
    // Create a dispatch queue for the HTTP requests
    _pollQueue = dispatch_queue_create("node-dss-polling", DISPATCH_QUEUE_SERIAL);
    
    // Create the peer connection
    mrsPeerConnectionConfiguration config{};
    config.sdp_semantic = mrsSdpSemantic::kUnifiedPlan;
    mrsPeerConnectionHandle handle = nullptr;
    mrsResult ret = mrsPeerConnectionCreate(&config, &handle);
    if (ret != mrsResult::kSuccess) {
      NSLog(@"Failed to create peer connection: error code #%u", ret);
      return nil;
    }
  }
  return self;
}

- (void)startPollingSignaler:(NSURL *)serverURL
             withLocalPeerId:(NSString *)localPeerId
            withRemotePeerId:(NSString *)remotePeerId {
  // Compute polling URL by appending the local peer ID to the server URL
  _getURL = [NSURL URLWithString:[NSString stringWithFormat:@"data/%@", localPeerId]
                   relativeToURL:serverURL];
  NSLog(@"GET URL: %@", _getURL.absoluteString);
  
  // Compute sending URL by appending the remote peer ID to the server URL
  _postURL = [NSURL URLWithString:[NSString stringWithFormat:@"data/%@", remotePeerId]
                    relativeToURL:serverURL];
  NSLog(@"POST URL: %@", _postURL.absoluteString);
  
  // Send the first poll request immediately
  dispatch_async(_pollQueue, ^{ [self sendPollRequest]; });
}

// Run on the polling queue
- (void)sendPollRequest {
  // Prepare the request
  NSURLSessionDataTask *getTask = [[NSURLSession sharedSession]
                                   dataTaskWithURL:_getURL
                                   completionHandler:^(NSData *data,
                                                       NSURLResponse *response,
                                                       NSError *error) {
                                     [self OnHTTPRequestData:data
                                                    response:(NSHTTPURLResponse *)response
                                                       error: error];
                                   }];
  // Send the request
  [getTask resume];
}

// Runs on the polling queue
- (void)OnHTTPRequestData:(NSData *)data
                 response:(NSHTTPURLResponse *)response
                    error:(NSError *)error {
  if (error != nil) {
    NSLog(@"HTTP request error: %@", error.localizedDescription);
    return;
  }
  
  if (response.statusCode == 404) {
    // No data - just ignore
    // Enqueue next request in 500ms
    dispatch_time_t nextTime = dispatch_time(DISPATCH_TIME_NOW, 500000000);
    dispatch_after(nextTime, _pollQueue, ^{ [self sendPollRequest]; });
    return;
  }
  
  NSString* jsonContent = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  NSLog(@"Received node-dss message: %@", jsonContent);
  
  // Deserialize JSON
  NSError *jsonError;
  NSDictionary *jsonData = [NSJSONSerialization
                            JSONObjectWithData:data
                            options:kNilOptions
                            error:&jsonError
                            ];
  if (jsonError == nil) {
    // Parse message
//    for (NSString *key in jsonData) {
//      id value = jsonData[key];
//      NSLog(@"Value: %@ for key: %@", value, key);
//    }
    SdpMessage msg;
    msg.messageType = (SdpMessageType)[jsonData[@"MessageType"] integerValue];
    msg.data = jsonData[@"Data"];
    
    // Dispatch to main thread for UI handling
    dispatch_async(dispatch_get_main_queue(), ^{ [self handleMessage:msg]; });
  } else {
    NSLog(@"Error deserializing JSON message: %@", jsonError.localizedDescription);
  }
  
  // Poll again immediately
  dispatch_async(_pollQueue, ^{ [self sendPollRequest]; });
}

// Runs on the main queue
- (void)handleMessage:(SdpMessage)message {
  NSLog(@"Received message: type=%d", message.messageType);
}

//static void runBlockOnMainThread(void (^block)(void))
//{
//  if (!block) return;
//  if ([[NSThread currentThread] isMainThread]) {
//    block();
//  } else {
//    dispatch_async(dispatch_get_main_queue(), block);
//  }
//}
//
//static void runFunctionOnMainThread(void (*func)(void*), void* arg)
//{
//  if (!func) return;
//  if ([[NSThread currentThread] isMainThread]) {
//    func(arg);
//  } else {
//    dispatch_async_f(dispatch_get_main_queue(), 0, func);
//  }
//}

@end
