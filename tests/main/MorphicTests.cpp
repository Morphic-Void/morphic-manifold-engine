//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include <iostream>

#include "tests/environment/test_environment.hpp"
#include "tests/environment/test_paths.hpp"
#include "tests/main/run_tests.hpp"

int main(const int argc, char** const argv)
{
    if (should_print_usage(argc, argv))
    {
        print_usage();
        return 0;
    }
    if (!test_environment::initialise_paths((argc > 0) ? argv[0] : nullptr))
    {
        std::cerr << "MorphicTests could not locate the repository test data.\n";
        return 1;
    }
    if (!test_environment::install())
    {
        std::cerr << "MorphicTests could not install its executable environment.\n";
        return 1;
    }

    const int result = run_tests(parse_test_run_mode(argc, argv));
    if (!test_environment::is_clean())
    {
        std::cerr << "MorphicTests finished with residual service or memory state.\n";
        return (result == 0) ? 1 : result;
    }
    return result;
}
