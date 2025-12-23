#include "Functions.h"
#include <iostream>
using namespace std;

int main(){
	cout << "Enter 2 numbers seperated by a space: ";
	
	int x{};
	int y{};
	double z{};
	
	cin >> x >> y;
	
	cout << x << " + " << y << " is: " << Add<int>(x, y) << endl;
	cout << x << " - " << y << " is: " << Subtract<int>(x, y) << endl;
	
	cout << "Enter another number: ";
	cin >> z;
	
	cout << x << " + " << y << " + " << z << " is: " << Add<double>(x, y, z) << endl;
	cout << x << " - " << y << " - " << z << " is: " << Subtract<double>(x, y, z) << endl;
	
	system("pause");
	return 0;
}
