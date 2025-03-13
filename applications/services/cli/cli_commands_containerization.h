#pragma once
#include "cli.h"

/**
 * @brief Initialize containerization CLI commands
 * 
 * @param cli CLI instance
 */
void cli_commands_containerization_init(Cli* cli);

/**
 * @brief Deinitialize containerization CLI commands
 * 
 * @param cli CLI instance
 */
void cli_commands_containerization_deinit(Cli* cli);
