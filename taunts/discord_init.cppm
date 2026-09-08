module;

#include <dpp/dpp.h>

export module voice_taunts_dc;

export import voice_taunts_dc.commands;
export import echterwachter;

export inline const int init_voice_taunts = []
{
    // Single top-level "/t" command with an integer parameter (not a "/t say"
    // subcommand) - matches AoE3's own "type a number in chat" taunt trigger,
    // and sidesteps Discord's 25-subcommand-per-group cap that 33 literal
    // "/t <n>" subcommands would have hit.
    dpp::slashcommand cmd("t", "Age of Empires III taunt lejatszasa a hangcsatornadban", bot.me.id);
    cmd.set_interaction_contexts({dpp::itc_guild, dpp::itc_bot_dm});
    cmd.set_dm_permission(true);

    dpp::command_option number_opt(dpp::co_integer, "number", "Taunt szama (1-33)", true);
    number_opt.set_min_value(int64_t{1}).set_max_value(int64_t{33});
    cmd.add_option(number_opt);

    add_command(BotCommand(cmd, std::nullopt, taunt_cmd));

    return 42;
}();
