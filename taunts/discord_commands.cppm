module;

#include <dpp/dpp.h>
#include <string>

export module voice_taunts_dc.commands;

export import voice_taunts;
export import echterwachter;

namespace
{
    void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& text)
    {
        event.reply(dpp::message(text).set_flags(dpp::m_ephemeral));
    }
}

export void taunt_cmd(const dpp::slashcommand_t& event)
{
    auto number_param = event.get_parameter("number");
    auto p = std::get_if<int64_t>(&number_param);
    if (!p)
    {
        reply_ephemeral(event, "Ervenytelen parameter: szamot varok (1-33)!");
        return;
    }
    int n = static_cast<int>(*p);

    auto user_id = event.command.get_issuing_user().id;

    dpp::snowflake guild_id = event.command.guild_id;
    if (guild_id == 0)
    {
        // Invoked from a DM with the bot: find a shared guild where the caller
        // is currently in a voice channel, since there's no guild context here.
        guild_id = vc::find_voice_guild(user_id);
        if (guild_id == 0)
        {
            reply_ephemeral(event, "Nem vagy hangcsatornaban egyik kozos szerverunkon sem!");
            return;
        }
    }

    if (!vc_taunts::say(event.from(), guild_id, user_id, n))
    {
        reply_ephemeral(event, "Nem vagy hangcsatornaban ezen a szerveren!");
        return;
    }

    reply_ephemeral(event, "Rendben, mondom: \"" + std::string(vc_taunts::lines[n - 1]) + "\"");
}
