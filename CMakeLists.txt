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

# Piper (https://github.com/rhasspy/piper) gives /vc say a natural neural voice
# instead of the robotic espeak-ng fallback. The engine binary itself is expected
# to already be installed at PIPER_DIR (see README), but its voice model is just a
# data file, so CMake fetches it automatically if it's missing.
set(PIPER_DIR "$ENV{HOME}/.local/share/piper")
set(PIPER_VOICE_DIR "${PIPER_DIR}/voices")
set(PIPER_HU_MODEL_BASENAME "hu_HU-anna-medium")
set(PIPER_HU_MODEL "${PIPER_VOICE_DIR}/${PIPER_HU_MODEL_BASENAME}.onnx")
set(PIPER_VOICES_BASE_URL "https://huggingface.co/rhasspy/piper-voices/resolve/main/hu/hu_HU/anna/medium")

foreach(_piper_ext "onnx" "onnx.json")
    set(_piper_dest "${PIPER_VOICE_DIR}/${PIPER_HU_MODEL_BASENAME}.${_piper_ext}")
    if(NOT EXISTS "${_piper_dest}")
        message(STATUS "Downloading Piper voice model file: ${PIPER_HU_MODEL_BASENAME}.${_piper_ext}")
        file(DOWNLOAD
                "${PIPER_VOICES_BASE_URL}/${PIPER_HU_MODEL_BASENAME}.${_piper_ext}"
                "${_piper_dest}"
                SHOW_PROGRESS
                STATUS _piper_dl_status
        )
        list(GET _piper_dl_status 0 _piper_dl_code)
        if(NOT _piper_dl_code EQUAL 0)
            list(GET _piper_dl_status 1 _piper_dl_message)
            message(WARNING "Could not download ${PIPER_HU_MODEL_BASENAME}.${_piper_ext} (${_piper_dl_message}) - /vc say will fall back to espeak-ng for 'hu'.")
            file(REMOVE "${_piper_dest}")
        endif()
    endif()
endforeach()

target_compile_definitions(voice PRIVATE PIPER_DIR="${PIPER_DIR}")
