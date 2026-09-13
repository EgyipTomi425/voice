module;

#include <dpp/dpp.h>
#include <string>

export module voice;

export namespace vc
{
    // Joins the voice channel that `user_id` is currently sitting in on `guild_id`
    // (unless the bot is already connected there) and queues `text` to be read out
    // loud via TTS once the connection is ready.
    //
    // Every guild gets its own queue drained by a single dedicated worker thread, so
    // concurrent callers on the same guild are serialized (queued one after another)
    // instead of racing to connect twice or talking over each other; callers on
    // different guilds never block one another.
    //
    // `lang` is an espeak-ng voice name (e.g. "hu", "en") - it picks the pronunciation
    // rules, so a Hungarian sentence read with the English voice comes out mangled.
    //
    // Returns false if `user_id` is not currently in any voice channel on `guild_id`.
    bool say
    (
        dpp::discord_client* shard,
        dpp::snowflake guild_id,
        dpp::snowflake user_id,
        const std::string& text,
        const std::string& lang = "hu"
    );

    // Searches every guild the bot shares with `user_id` for one where they're
    // currently sitting in a voice channel. Lets /vc say work from a DM with the
    // bot, without the caller having to go find a text channel on that server.
    // Returns 0 if `user_id` isn't in a voice channel on any shared guild.
    dpp::snowflake find_voice_guild(dpp::snowflake user_id);
}
