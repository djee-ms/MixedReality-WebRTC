//
//  ViewController.m
//  testapp
//
//  Created by Jérôme HUMBERT on 03/02/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#import "ViewController.h"

@interface ViewController ()
@property (strong, nonatomic) IBOutlet UILabel *titleLabel;
@property (strong, nonatomic) IBOutlet UITextField *serverTextField;

@end

@implementation ViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
}

//- (IBAction)OnConnectButton:(id)sender {
//	_titleLabel.text = @"test!";
//	_serverTextField.text = @"test2!";
//}

- (IBAction)OnConnectButtonClicked:(UIButton *)sender {
	_titleLabel.text = @"test!";
	_serverTextField.text = @"test2!";
}

@end
