#include "df0.hpp"
#include "openrouter_transcribe.h"
#include <iostream>
#include <sstream>
int main(int argc, char* argv[])
{
  using namespace df0;
  std::stringstream out_stream;

  try
  {
    std::filesystem::path("data_processing_definition.mp3") | openrouter::transcribe(environment::get("OPENROUTER_API_KEY").value(), "openai/whisper-large-v3") | plain_text_exporter() | out_stream;
    ensure(fuzzy_match::ratio(out_stream.str(), "Data processing involves converting raw data into meaningful information by collecting, organizing, analyzing, and interpreting it. This process includes tasks like sorting, filtering, summarizing, and transforming data using computational and statistical methods. It is crucial in fields like business, science, and technology, as it helps organizations extract valuable insights from large datasets, make informed decisions, and enhance efficiency.\n")) > 50;
  }
  catch (const std::exception& e)
  {
    std::cerr << errors::diagnostic_message(e) << std::endl;
    return 1;
  }

  return 0;
}
