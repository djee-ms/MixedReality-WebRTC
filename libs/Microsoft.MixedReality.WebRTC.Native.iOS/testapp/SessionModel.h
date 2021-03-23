//
//  SessionModel.h
//  mr-webrtc-testapp
//
//  Created by Jérôme HUMBERT on 15/06/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SessionModel : NSObject

/// Connect to the given node-dss server.
- (void)startPollingSignaler:(NSURL *)serverURL
             withLocalPeerId:(NSString *)localPeerId
            withRemotePeerId:(NSString *)remotePeerId;

@end

NS_ASSUME_NONNULL_END
