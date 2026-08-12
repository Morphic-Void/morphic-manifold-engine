
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   run_tests.cpp
//  Author: Ritchie Brannan
//  Date:   24 Apr 26

#include "tests/run_tests.hpp"
#include "tests/AssetRepository_test_suite.hpp"
#include "tests/AsyncState_test_suite.hpp"
#include "tests/ByteBuffers_test_suite.hpp"
#include "tests/CMemoryToken_test_suite.hpp"
#include "tests/CMemoryView_test_suite.hpp"
#include "tests/DebugService_test_suite.hpp"
#include "tests/ErasedPod_test_suite.hpp"
#include "tests/ErasedOwner_test_suite.hpp"
#include "tests/StringBuffers_test_suite.hpp"
#include "tests/SystemTypeIdentity_test_suite.hpp"
#include "tests/TInstance_test_suite.hpp"
#include "tests/TOrderedCollection_test_suite.hpp"
#include "tests/TPodFifo_test_suite.hpp"
#include "tests/TPodVector_test_suite.hpp"
#include "tests/TUnorderedCollection_test_suite.hpp"
#include "tests/TOrderedSlots_test_harness.hpp"
#include "tests/TUnorderedSlots_test_harness.hpp"
#include "tests/TQueueTransport_test_suite.hpp"
#include "tests/TMpmcTransport_test_suite.hpp"
#include "tests/TRingTransport_test_suite.hpp"
#include "tests/TOwningTransport_test_suite.hpp"

#include <cstring>
#include <iostream>
#include <string>

namespace
{
bool starts_with(const std::string& value, const char* prefix)
{
    const std::size_t prefix_length = std::strlen(prefix);
    return value.size() >= prefix_length && std::memcmp(value.data(), prefix, prefix_length) == 0;
}

bool is_help_argument(const std::string& argument)
{
    return
        argument == "-?" ||
        argument == "/?" ||
        argument == "-h" ||
        argument == "--help" ||
        argument == "-help" ||
        argument == "/help";
}

bool parse_test_mode_value(const std::string& value, ETestRunMode& out_mode)
{
    if (value == "0")
    {
        out_mode = ETestRunMode::none;
        return true;
    }
    if (value == "1")
    {
        out_mode = ETestRunMode::core;
        return true;
    }
    if (value == "2")
    {
        out_mode = ETestRunMode::moderate;
        return true;
    }
    if (value == "3")
    {
        out_mode = ETestRunMode::full;
        return true;
    }
    return false;
}
}

bool should_print_usage(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (is_help_argument(argument))
        {
            return true;
        }
    }

    return false;
}

void print_usage()
{
    std::cout <<
        "Usage: MorphicEngine [options]\n"
        "\n"
        "Options:\n"
        "  -?, /?, -h, --help, -help, /help\n"
        "               Show this usage summary\n"
        "  -t0          Skip all tests\n"
        "  -t1          Run core tests only (no expensive harness work)\n"
        "  -t2          Run moderate tests (current default)\n"
        "  -t3          Run full expensive tests\n"
        "  --tests=<0-3>\n"
        "               Long-form equivalent of -t0..-t3\n";
}

ETestRunMode parse_test_run_mode(int argc, char** argv)
{
    ETestRunMode mode = ETestRunMode::moderate;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if (argument.size() == 3 && starts_with(argument, "-t"))
        {
            ETestRunMode parsed_mode = mode;
            if (parse_test_mode_value(argument.substr(2), parsed_mode))
            {
                mode = parsed_mode;
            }
            continue;
        }

        if (starts_with(argument, "--tests="))
        {
            ETestRunMode parsed_mode = mode;
            if (parse_test_mode_value(argument.substr(std::strlen("--tests=")), parsed_mode))
            {
                mode = parsed_mode;
            }
            continue;
        }
    }

    return mode;
}

int run_tests(ETestRunMode mode)
{
    if (mode == ETestRunMode::none)
    {
        return 0;
    }

    int cumulative_result = 0;

    int asset_repository_test_result = run_asset_repository_tests();
    cumulative_result += asset_repository_test_result;

    int async_state_test_result = run_async_state_tests();
    cumulative_result += async_state_test_result;

    int memory_token_test_result = run_memory_token_tests();
    cumulative_result += memory_token_test_result;

    int memory_view_test_result = run_memory_view_tests();
    cumulative_result += memory_view_test_result;

    int erased_owner_test_result = run_erased_owner_tests();
    cumulative_result += erased_owner_test_result;

    int erased_pod_test_result = run_erased_pod_tests();
    cumulative_result += erased_pod_test_result;

    int pod_vector_test_result = run_pod_vector_tests();
    cumulative_result += pod_vector_test_result;

    int byte_buffer_test_result = run_byte_buffer_tests();
    cumulative_result += byte_buffer_test_result;

    int instance_test_result = run_instance_tests();
    cumulative_result += instance_test_result;

    int debug_service_test_result = run_debug_service_tests();
    cumulative_result += debug_service_test_result;

    int system_type_identity_test_result = run_system_type_identity_tests();
    cumulative_result += system_type_identity_test_result;

    int pod_fifo_test_result = run_pod_fifo_tests();
    cumulative_result += pod_fifo_test_result;

    int string_buffer_test_result = run_string_buffer_tests();
    cumulative_result += string_buffer_test_result;

    int ordered_collection_test_result = run_ordered_collection_tests();
    cumulative_result += ordered_collection_test_result;

    int unordered_collection_test_result = run_unordered_collection_tests();
    cumulative_result += unordered_collection_test_result;

    int towning_test_result = run_owning_transport_tests();
    cumulative_result += towning_test_result;

    int tqueue_test_result = run_queue_transport_tests();
    cumulative_result += tqueue_test_result;

    int tring_test_result = run_ring_transport_tests();
    cumulative_result += tring_test_result;

    int mpmc_transport_test_result = run_mpmc_transport_tests();
    cumulative_result += mpmc_transport_test_result;

    if (mode >= ETestRunMode::core)
    {
        TOrderedConfig tlex_cfg;
        if (mode >= ETestRunMode::moderate)
        {
            tlex_cfg.run_fuzz_lightweight = true;
        }
        if (mode >= ETestRunMode::full)
        {
            tlex_cfg.run_exhaustive_insert_delete = true;
            tlex_cfg.max_insert_perms = 0;
            tlex_cfg.max_delete_perms_each_insert = 0;
            tlex_cfg.run_fuzz_lightweight = true;
        }

        int tlex_test_result = run_all_tests(tlex_cfg);
        cumulative_result += tlex_test_result;
    }

    if (mode >= ETestRunMode::core)
    {
        TUnorderedConfig tun_cfg;
        if (mode >= ETestRunMode::full)
        {
            tun_cfg.run_fuzz = true;
        }

        int tun_test_result = run_all_tests(tun_cfg);
        cumulative_result += tun_test_result;
    }

    return cumulative_result;
}
