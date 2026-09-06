module;

#include <dpp/dpp.h>
#include <string>

export module voice_dc.commands;

export import voice;
export import echterwachter;

namespace
{
    void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& text)
    {
        event.reply(dpp::message(text).set_flags(dpp::m_ephemeral));
    }
}

export void say_cmd(const dpp::slashcommand_t& event)
{
    std::string text;
    auto text_param = event.get_parameter("text");
    if (auto p = std::get_if<std::string>(&text_param))
        text = *p;
    else
    {
        reply_ephemeral(event, "Ervenytelen parameter: szoveget varok!");
        return;
    }

    std::string lang = "hu";
    auto lang_param = event.get_parameter("lang");
    if (auto p = std::get_if<std::string>(&lang_param); p && !p->empty())
        lang = *p;

    if (event.command.guild_id == 0)
    {
        reply_ephemeral(event, "Ez a parancs csak szerveren mukodik, DM-ben nem.");
        return;
    }

    auto user_id = event.command.get_issuing_user().id;

    if (!vc::say(event.from(), event.command.guild_id, user_id, text, lang))
    {
        reply_ephemeral(event, "Nem vagy hangcsatornaban ezen a szerveren!");
        return;
    }

    reply_ephemeral(event, "Rendben, felolvasom a hangcsatornadban!");
}
