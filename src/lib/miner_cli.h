#pragma once

#include "miner_config.h"

#include <string>

enum class CliParseResult {
	Ok,
	HelpShown,
	Error
};

void print_usage();
CliParseResult parse_args(int argc, char** argv, std::string& config_path, MinerConfig& cfg);
