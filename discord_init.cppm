module;

#include <dpp/dpp.h>

export module voice_dc;

export import voice_dc.commands;
export import echterwachter;

export inline const int init_voice = []
{
    CommandGroup vc_commands
    (
        "vc",
        "Voice channel related commands"
    );

    // Allow /vc say both in a server and in a DM with the bot (DM callers get
    // routed to whichever shared guild they're currently in voice on).
    vc_commands.cmd.set_interaction_contexts({dpp::itc_guild, dpp::itc_bot_dm});
    vc_commands.cmd.set_dm_permission(true);

    vc_commands.add
    (
        "say", "Csatlakozik a hangcsatornadhoz es felolvassa a szoveget", say_cmd,
            params
            (
                "text"_str,
                string_param("lang", "espeak-ng hangkod, pl. hu / en (alapertelmezett: hu)", false)
            )
    );

    vc_commands.register_commands();

    return 42;
}();
