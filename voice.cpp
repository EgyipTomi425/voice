module;

#include <dpp/dpp.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

module voice;

import echterwachter;

namespace
{
    struct queued_line
    {
        std::string text;
        std::string lang;
        dpp::snowflake user_id;
    };

    struct guild_voice_session
    {
        std::mutex mtx;
        std::condition_variable cv;
        std::deque<queued_line> queue;
        bool worker_running = false;
        bool ready = false;
    };

    std::mutex sessions_mutex;
    std::unordered_map<dpp::snowflake, std::shared_ptr<guild_voice_session>> sessions;

    std::shared_ptr<guild_voice_session> get_or_create_session(dpp::snowflake guild_id)
    {
        std::lock_guard lock(sessions_mutex);
        auto& s = sessions[guild_id];
        if (!s)
            s = std::make_shared<guild_voice_session>();
        return s;
    }

    std::shared_ptr<guild_voice_session> find_session(dpp::snowflake guild_id)
    {
        std::lock_guard lock(sessions_mutex);
        auto it = sessions.find(guild_id);
        return it != sessions.end() ? it->second : nullptr;
    }

    // Runs `argv` directly via fork+execvp (no shell involved anywhere), feeds it
    // `input` on stdin, and returns everything it wrote to stdout. Returns an empty
    // vector if the process could not be started or exited with a non-zero status.
    // Because there is no shell in the loop, user-supplied text in `argv` can never
    // be interpreted as shell syntax (no command injection risk).
    // `env_overrides` are set in the child only (e.g. piper needs LD_LIBRARY_PATH
    // pointed at its own directory for its bundled .so's).
    std::vector<uint8_t> run_process
    (
        const std::vector<std::string>& argv,
        const std::vector<uint8_t>& input,
        const std::vector<std::pair<std::string, std::string>>& env_overrides = {}
    )
    {
        int in_pipe[2];
        int out_pipe[2];
        if (pipe(in_pipe) != 0)
            return {};
        if (pipe(out_pipe) != 0)
        {
            close(in_pipe[0]);
            close(in_pipe[1]);
            return {};
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            close(in_pipe[0]); close(in_pipe[1]);
            close(out_pipe[0]); close(out_pipe[1]);
            return {};
        }

        if (pid == 0)
        {
            dup2(in_pipe[0], STDIN_FILENO);
            dup2(out_pipe[1], STDOUT_FILENO);
            close(in_pipe[0]); close(in_pipe[1]);
            close(out_pipe[0]); close(out_pipe[1]);

            for (auto& [key, value] : env_overrides)
                setenv(key.c_str(), value.c_str(), 1);

            std::vector<char*> c_argv;
            c_argv.reserve(argv.size() + 1);
            for (auto& s : argv)
                c_argv.push_back(const_cast<char*>(s.c_str()));
            c_argv.push_back(nullptr);

            execvp(c_argv[0], c_argv.data());
            _exit(127);
        }

        close(in_pipe[0]);
        close(out_pipe[1]);

        std::jthread writer([&]
        {
            size_t written = 0;
            while (written < input.size())
            {
                ssize_t n = write(in_pipe[1], input.data() + written, input.size() - written);
                if (n <= 0)
                    break;
                written += static_cast<size_t>(n);
            }
            close(in_pipe[1]);
        });

        std::vector<uint8_t> output;
        std::array<uint8_t, 4096> buf{};
        ssize_t n;
        while ((n = read(out_pipe[0], buf.data(), buf.size())) > 0)
            output.insert(output.end(), buf.begin(), buf.begin() + n);
        close(out_pipe[0]);

        writer.join();

        int status = 0;
        waitpid(pid, &status, 0);
        if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0))
            return {};

        return output;
    }

    // Piper is a small neural (VITS) TTS engine - it sounds much more natural than
    // the formant-synthesis espeak-ng voices, and still runs entirely locally/
    // offline (fine on a Raspberry Pi). Install: https://github.com/rhasspy/piper
    // PIPER_DIR (CMAKE_CURRENT_SOURCE_DIR/CMakeLists.txt: "$ENV{HOME}/.local/share/piper")
    // is where the `piper` binary is expected to already be installed; CMake fetches
    // missing voice models there automatically - see that CMakeLists.txt to add more
    // languages once a model's .onnx/.onnx.json pair has a download rule.
    constexpr const char* piper_dir = PIPER_DIR;
    const std::string piper_bin = std::string(piper_dir) + "/piper";

    std::string piper_model_for_lang(const std::string& lang)
    {
        static const std::unordered_map<std::string, std::string> models
        {
            {"hu", std::string(piper_dir) + "/voices/hu_HU-anna-medium.onnx"},
            {"en", std::string(piper_dir) + "/voices/en_US-amy-medium.onnx"},
        };
        auto it = models.find(lang);
        return it != models.end() ? it->second : std::string{};
    }

    // Synthesizes `text` as a WAV with Piper's neural voice for `lang`. Text is fed
    // on stdin (never through a shell, never as an argv element), so it can't be
    // interpreted as anything but speech. Returns {} if no Piper model is installed
    // for `lang`, or if synthesis failed.
    std::vector<uint8_t> synthesize_with_piper(const std::string& text, const std::string& lang)
    {
        auto model = piper_model_for_lang(lang);
        if (model.empty())
            return {};

        std::vector<uint8_t> input(text.begin(), text.end());
        return run_process
        (
            {piper_bin, "--model", model, "--output_file", "-"},
            input,
            {{"LD_LIBRARY_PATH", piper_dir}}
        );
    }

    // Fallback for languages with no installed Piper model: espeak-ng's formant
    // synthesis is far more robotic, but it covers dozens of languages out of the
    // box. `text`/`lang` are passed as single argv elements (never through a shell),
    // so they cannot break out into shell syntax; the leading "--" also stops
    // espeak-ng from treating a message that starts with "-" as an option.
    std::vector<uint8_t> synthesize_with_espeak(const std::string& text, const std::string& lang)
    {
        return run_process({"espeak-ng", "-v", lang, "--stdout", "--", text}, {});
    }

    // Synthesizes `text` (in voice/language `lang`, e.g. "hu"/"en") to raw PCM:
    // signed 16-bit little-endian, 48000 Hz, stereo (exactly what
    // dpp::discord_voice_client::send_audio_raw expects). ffmpeg itself cannot do
    // text-to-speech - it's only used here to resample/reformat whichever engine's
    // WAV output into the PCM layout Discord needs.
    std::vector<uint8_t> text_to_pcm(const std::string& text, const std::string& lang)
    {
        auto wav = synthesize_with_piper(text, lang);
        if (wav.empty())
            wav = synthesize_with_espeak(text, lang);
        if (wav.empty())
            return {};

        return run_process
        (
            {
                "ffmpeg", "-hide_banner", "-loglevel", "error",
                "-i", "pipe:0",
                "-f", "s16le", "-ar", "48000", "-ac", "2", "pipe:1"
            },
            wav
        );
    }

    // Failures are DMed to the requesting user rather than posted in the guild
    // channel, so - like the slash command reply itself - only that user sees them.
    void notify_user(dpp::snowflake user_id, const std::string& text)
    {
        bot.direct_message_create(user_id, dpp::message(text));
    }

    void worker_loop
    (
        std::shared_ptr<guild_voice_session> session,
        dpp::discord_client* shard,
        dpp::snowflake guild_id
    )
    {
        while (true)
        {
            queued_line line;
            {
                std::unique_lock lock(session->mtx);
                if (session->queue.empty())
                {
                    session->worker_running = false;
                    return;
                }
                line = std::move(session->queue.front());
                session->queue.pop_front();
            }

            {
                std::unique_lock lock(session->mtx);

                // The connection may already be ready without us ever having
                // seen its voice_ready event - e.g. /join connected the bot
                // directly through dpp before this module's on_voice_ready
                // hook was even registered (that only happens lazily, on the
                // first /vc say or /t). Trust dpp's own live state over our
                // bookkeeping so we don't wait on a flag that will never
                // flip for a connection that's already up.
                if (!session->ready)
                {
                    dpp::voiceconn* v = shard->get_voice(guild_id);
                    if (v && v->voiceclient && v->voiceclient->is_ready())
                        session->ready = true;
                }

                bool became_ready = session->cv.wait_for(lock, std::chrono::seconds(15), [&] { return session->ready; });
                if (!became_ready)
                {
                    notify_user(line.user_id, "Nem sikerult csatlakozni a hangcsatornahoz (idotullepes).");
                    continue;
                }
            }

            auto pcm = text_to_pcm(line.text, line.lang);
            if (pcm.empty())
            {
                notify_user(line.user_id, "A felolvasas nem sikerult. Telepitve van az espeak-ng es az ffmpeg, es ismert a megadott nyelv?");
                continue;
            }

            dpp::voiceconn* v = shard->get_voice(guild_id);
            if (!v || !v->voiceclient || !v->voiceclient->is_ready())
            {
                notify_user(line.user_id, "A hangkapcsolat idokozben megszunt.");
                continue;
            }

            v->voiceclient->send_audio_raw(reinterpret_cast<uint16_t*>(pcm.data()), pcm.size());
        }
    }

    void ensure_voice_ready_hook_registered()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            bot.on_voice_ready([](const dpp::voice_ready_t& event)
            {
                auto session = find_session(event.voice_client->server_id);
                if (!session)
                    return;

                std::lock_guard lock(session->mtx);
                session->ready = true;
                session->cv.notify_all();
            });
        });
    }
}

namespace vc
{
    bool say
    (
        dpp::discord_client* shard,
        dpp::snowflake guild_id,
        dpp::snowflake user_id,
        const std::string& text,
        const std::string& lang
    )
    {
        ensure_voice_ready_hook_registered();

        dpp::guild* g = dpp::find_guild(guild_id);
        if (!g)
            return false;

        auto session = get_or_create_session(guild_id);

        std::unique_lock lock(session->mtx);

        dpp::voiceconn* existing = shard->get_voice(guild_id);

        dpp::snowflake target_channel_id = 0;
        auto vsi = g->voice_members.find(user_id);
        if (vsi != g->voice_members.end())
            target_channel_id = vsi->second.channel_id;

        // Reconnect not just when there's no voice connection yet, but also when
        // there is one and the requesting user is currently in a *different*
        // channel - otherwise the bot would keep talking in whatever channel it
        // joined first instead of following the user who ran the command.
        bool need_connect = !existing || (target_channel_id != 0 && existing->channel_id != target_channel_id);

        if (need_connect)
        {
            if (!g->connect_member_voice(bot, user_id))
                return false;
            session->ready = false;
        }

        session->queue.push_back(queued_line{text, lang, user_id});

        if (session->worker_running)
            return true;

        session->worker_running = true;
        lock.unlock();

        std::jthread([session, shard, guild_id]
        {
            worker_loop(session, shard, guild_id);
        }).detach();

        return true;
    }

    dpp::snowflake find_voice_guild(dpp::snowflake user_id)
    {
        dpp::cache<dpp::guild>* c = dpp::get_guild_cache();
        auto& container = c->get_container();
        std::shared_lock lock(c->get_mutex());

        for (auto& [id, g] : container)
            if (g->voice_members.find(user_id) != g->voice_members.end())
                return id;

        return 0;
    }
}
