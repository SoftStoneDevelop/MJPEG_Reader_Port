#pragma once

#include <iostream>
#include "first_app.hpp"

int main(int argc, char* argv[])
{
    lve::FirstApp app{};

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}