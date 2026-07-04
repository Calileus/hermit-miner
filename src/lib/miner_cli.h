#pragma once

#include "miner_config.h"

#include <string>

void print_usage();
bool parse_args(int argc, char** argv, std::string& config_path, MinerConfig& cfg);
