#include <iostream>

// Test to understand the button name issue

int main() {
    // Simulate what happens in the loop
    
    std::string idStr = "";
    
    // First button
    idStr = "1000";
    std::string name1 = "2 Players" + idStr;
    std::cout << "Button 1 name: " << name1 << std::endl;
    
    // Second button  
    idStr = "1001";
    std::string name2 = "4 Players" + idStr;
    std::cout << "Button 2 name: " << name2 << std::endl;
    
    // Check if they're unique
    std::cout << "Are they different? " << (name1 != name2 ? "YES" : "NO") << std::endl;
    
    return 0;
}
