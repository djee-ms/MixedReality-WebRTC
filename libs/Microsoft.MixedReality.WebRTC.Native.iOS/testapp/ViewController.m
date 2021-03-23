//
//  ViewController.m
//  testapp
//
//  Created by Jérôme HUMBERT on 03/02/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import "SessionModel.h"
#import "ViewController.h"

@interface ViewController()

@property (strong, nonatomic) IBOutlet UILabel *titleLabel;
@property (strong, nonatomic) IBOutlet UIButton *connectButton;
@property (strong, nonatomic) IBOutlet UITextField *serverTextField;
@property (strong, nonatomic) IBOutlet UITextField *localPeerTextField;
@property (strong, nonatomic) IBOutlet UITextField *remotePeerTextField;

@end

@implementation ViewController

- (void)viewDidLoad {
	[super viewDidLoad];
  // Change text of button when it is disabled (while polling)
  [_connectButton setTitle:@"Connecting..." forState:UIControlStateDisabled];
}

//- (IBAction)OnConnectButton:(id)sender {
//	_titleLabel.text = @"test!";
//	_serverTextField.text = @"test2!";
//}

- (IBAction)OnConnectButtonClicked:(UIButton *)sender {
  // IDEALLY YES, BUT DONT DISABLE FOR DEBUGGING
  //_connectButton.enabled = FALSE;
  
  // Start polling
  // TODO - Abstract signaler
  NSURL *serverURL = [NSURL URLWithString:_serverTextField.text];
  [self.sessionModel startPollingSignaler:serverURL
                          withLocalPeerId:_localPeerTextField.text
                         withRemotePeerId:_remotePeerTextField.text];
}

@end
