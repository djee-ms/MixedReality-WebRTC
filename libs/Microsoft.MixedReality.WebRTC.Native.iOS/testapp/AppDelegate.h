//
//  AppDelegate.h
//  testapp
//
//  Created by Jérôme HUMBERT on 03/02/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SessionModel;

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) SessionModel *sessionModel;

@end

