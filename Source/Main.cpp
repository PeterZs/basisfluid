
#include "Application.h"
#include <iostream>

using namespace std;

Application* app = nullptr;

int main()
{
    app = new Application();
    app->Run();

    return 0;
}
