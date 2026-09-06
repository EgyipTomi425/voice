add_library(voice)

message(STATUS "VOICE = ${VOICE}")
message(STATUS "DISCORD = ${DISCORD}")

target_sources(voice
        PUBLIC
        FILE_SET CXX_MODULES FILES
            voice.cppm
        PRIVATE
            voice.cpp
)

target_link_libraries(voice PUBLIC echterwachter)

if(DISCORD)
    target_sources(voice
            PUBLIC
            FILE_SET CXX_MODULES FILES
                discord_commands.cppm
                discord_init.cppm
    )
endif()

target_compile_definitions(voice PUBLIC VOICE)
