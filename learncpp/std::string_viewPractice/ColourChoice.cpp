#include <iostream>
#include <string>

int main () {
	
	std::cout << "Pick Red or Blue (r/b): ";
	std::string choice {};
	std::cin >> choice;
	
	std::cout << "Now enter your name: ";
	std::string name {};
	std::getline(std::cin >> std::ws, name);
	
	std::cout << "Enter a number: ";
	int num {};
	std::cin >> num;
	
	std::cout << "Hello " << name << ", The colour you picked is " << choice << "\n";
	std::cout << "Your name has " << name.length() << " characters" << "\n";
	std::cout << "The number of characters in your name ("<< name.length() << ") added to the number you picked ("<< num << ") is: " << static_cast<int>(name.length()) + num << "\n";
	
	

	system("pause");
	return 0;
}
