module;

#include <dpp/dpp.h>
#include <array>
#include <string>
#include <string_view>

export module voice_taunts;

export import voice;

export namespace vc_taunts
{
    // Age of Empires III's 33 built-in chat taunts, in their original chat-command
    // order (typing "1" in an AoE3 match's chat says "Yes", "2" says "No", etc.).
    // Non-verbal stage directions (laughter, trumpet fanfare, ...) are trimmed from
    // the text below, since there is no recorded AoE3 audio here - /vc say's
    // espeak-ng/Piper pipeline is reused to read the line out loud instead.
    inline constexpr std::array<std::string_view, 33> lines
    {
        "Yes",
        "No",
        "I need food.",
        "I need wood.",
        "I need coin.",
        "Do you have extra resources?",
        "I have extra food.",
        "I have extra wood.",
        "I have extra coin.",
        "Meet here.",
        "Are you ready?",
        "I need help!",
        "Attack now!",
        "Upgrade your trade route.",
        "Wololo.",
        "I'm in your base, killin' your dudes.",
        "Check in your wallet; that's me on the dollar bill.",
        "I believe that makes me your daddy.",
        "L-O-L, I am R-O-T-F-L!",
        "Aren't you becoming quite the little problem?",
        "Ha-hahahahaha!",
        "This will give me cred, street cred!",
        "Hey, shut your pie hole.",
        "I'll take that trade.",
        "You sit on the computer all day! You know nothing of hard life!",
        "Really? Such a noob.",
        "Ask not for whom the timer ticks; it ticks for thee.",
        "Hoi-hoi!",
        "Check in your pocket; the quarter is me too.",
        "Where is my mother?",
        "Charge!",
        "Believe it, little boy!",
        "Zing!",
    };

    // Queues taunt number `n` (1-33, matching AoE3's own chat-command numbering) to
    // be read out loud in `user_id`'s voice channel on `guild_id`, same mechanism as
    // /vc say. Returns false for an out-of-range `n` or if `user_id` isn't in a
    // voice channel on `guild_id`.
    inline bool say(dpp::discord_client* shard, dpp::snowflake guild_id, dpp::snowflake user_id, int n)
    {
        if (n < 1 || n > static_cast<int>(lines.size()))
            return false;

        return vc::say(shard, guild_id, user_id, std::string(lines[n - 1]), "en");
    }
}
