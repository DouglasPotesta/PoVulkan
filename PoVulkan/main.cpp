#include <memory>
#include <iostream>

#include "PoApp.h"


int main()
{
	try
	{
		NPoAppBehavior::run();
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return 0;
}