#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Test nested arrays in dictionaries
        NSArray *skills = @[@"Objective-C", @"Swift", @"Python"];
        NSNumber *count = @4;
        NSNumber *largeNumber = @12345678;
        
        NSDictionary *personInfo = @{
            @"name": @"Alice Smith",
            @"skills": skills,
            @"years_experience": count,
            @"employee_id": largeNumber
        };
        
        // Test deeper nesting
        NSArray *transactions = @[@100, @-50, @200, @-25];
        NSDictionary *accountSummary = @{
            @"account_holder": @"Bob Jones", 
            @"total_transactions": @([transactions count]),
            @"recent_transactions": transactions,
            @"balance": @1225.50
        };
        
        // Test very deep nesting
        NSDictionary *complexData = @{
            @"user_profile": personInfo,
            @"account_data": accountSummary,
            @"metadata": @{
                @"version": @1,
                @"timestamp": @1234567890,
                @"tags": @[@"test", @"nested", @"data"]
            }
        };
        
        // Set breakpoint here to test formatters
        NSLog(@"Testing nested collections:");  // Line 37
        NSLog(@"personInfo: %@", personInfo);
        NSLog(@"accountSummary: %@", accountSummary);
        NSLog(@"complexData: %@", complexData);
        
        // Test accessing nested values
        NSArray *userSkills = personInfo[@"skills"];
        NSNumber *transactionCount = accountSummary[@"total_transactions"];
        
        NSLog(@"Skills array has %lu elements", (unsigned long)[userSkills count]);
        NSLog(@"Transaction count value: %@", transactionCount);
        
        return 0;
    }
}