// Test program to verify expected dictionary format in LLDB
// Compile: clang++ -g -O0 test_dictionary_format.cpp -o test_dictionary_format

#include <iostream>
#include <map>
#include <string>

int main() {
    // Test case 1: Simple dictionary equivalent
    std::map<std::string, std::string> personInfo;
    personInfo["name"] = "John Doe";
    personInfo["occupation"] = "Developer";
    personInfo["city"] = "New York";
    
    // Test case 2: Mixed value types  
    std::map<std::string, int> scores;
    scores["math"] = 95;
    scores["physics"] = 87;
    scores["chemistry"] = 91;
    
    // Test case 3: Nested structure
    std::map<std::string, std::map<std::string, std::string>> nested;
    nested["user1"]["name"] = "Alice";
    nested["user1"]["role"] = "Admin";
    nested["user2"]["name"] = "Bob"; 
    nested["user2"]["role"] = "User";
    
    std::cout << "Set breakpoint here to test dictionary display" << std::endl;
    
    // Expected LLDB output format:
    // (lldb) p personInfo
    // {
    //   [0] = {name = "John Doe"}
    //   [1] = {occupation = "Developer"}  
    //   [2] = {city = "New York"}
    // }
    //
    // NOT the current format:
    // {
    //   [0].key = "name"
    //   [0].value = "John Doe"
    //   [1].key = "occupation"
    //   [1].value = "Developer"
    // }
    
    return 0;
}