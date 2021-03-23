//
//  NodeDssSignaler.h
//  mr-webrtc-testapp
//
//  Created by Jérôme HUMBERT on 16/06/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NodeDssSignaler : NSObject

- (void)startPollingServer:(NSURL *)server
           withLocalPeerId:(NSString *)localPeerId;

@end

NS_ASSUME_NONNULL_END
