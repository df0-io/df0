/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#include "df0.hpp"
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char* argv[])
{
  using namespace df0;
  std::stringstream out_stream;

  try
  {
    const std::string text = "Data processing involves converting raw data into meaningful information.";
    data_source{text, mime_type{"text/plain"}, confidence::highest} | openrouter::text_to_speech(environment::get("OPENROUTER_API_KEY").value(), "x-ai/grok-voice-tts-1.0", "eve", "mp3") | out_stream;
    ensure(out_stream.str().size()) > 1000;
    const std::string audio = out_stream.str();
    const auto byte_at = [&audio](std::size_t i) { return static_cast<unsigned char>(audio[i]); };
    const bool has_id3_tag = audio.compare(0, 3, "ID3") == 0;
    const bool has_mp3_frame_sync = byte_at(0) == 0xFF && (byte_at(1) & 0xE0) == 0xE0;
    ensure(has_id3_tag || has_mp3_frame_sync) == true;
  }
  catch (const std::exception& e)
  {
    std::cerr << errors::diagnostic_message(e) << std::endl;
    return 1;
  }

  return 0;
}
