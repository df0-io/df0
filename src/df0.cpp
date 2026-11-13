/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#include "ai_runner.h"
#include "model_chain_element.h"
#include "model_inference_config.h"
#ifdef DF0_CT2
#include "ct2_runner.h"
#include "local_ai_ct2_embed.h"
#include "local_ai_ct2_runner_factory.h"
#endif
#ifdef DF0_LLAMA
#include "llama_runner.h"
#include "local_ai_llama_runner_factory.h"
#endif
#ifdef DF0_LOCAL_AI
#include "local_ai_task.h"
#endif
#include "ai_elements.h"
#include <boost/program_options.hpp>
#include <functional>
#include <initializer_list>
#include <memory>
#include <fstream>
#include "content_type.h"
#include "csv_exporter.h"
#include "archives_parser.h"
#include "html_exporter.h"
#include "language.h"
#include "audit.hpp"
#include "audit/json_stream_auditor.hpp"
#include "ref_or_owned.h"
#include <magic_enum/magic_enum_iostream.hpp>
#include "mail_parser.h"
#include "meta_data_exporter.h"
#include "ocr_parser.h"
#include "office_formats_parser.h"
#include "openrouter_analyze_data.h"
#include "openrouter_classify.h"
#include "openrouter_detect_sentiment.h"
#include "openrouter_embed.h"
#include "openrouter_extract_entities.h"
#include "openrouter_extract_keywords.h"
#include "openrouter_find.h"
#include "openrouter_summarize.h"
#include "openrouter_task.h"
#include "openrouter_text_to_speech.h"
#include "openrouter_transcribe.h"
#include "openrouter_translate_to.h"
#include "pipeline/output_element.hpp"
#include "plain_text_exporter.h"
#include "post.h"
#include "resource_path.h"
#include "standard_filter.h"
#include "version.h"
#include "pipeline/chain_expression.hpp"
#include "pipeline/input_element.hpp"
#include "pipeline/function_transformer.hpp"
#include "pipeline/variant_expression.hpp"
#include <set>
#include <variant>

using namespace df0;

static void readme()
{
  // warning TODO: Generate readme string literal automatically.
  std::cout <<
	"DF-0: Data processing Framework with Zero-overhead\n"
	"\n"
	"Copyright (c) DF-0 Team, project website https://github.com/df0-io/df0\n"
	"SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial\n";
}

static void version()
{
  readme();
  std::cout << std::endl << "Version: " << VERSION << std::endl;
}

namespace df0
{
	using magic_enum::istream_operators::operator>>;
	using magic_enum::ostream_operators::operator<<;
}

namespace df0::openrouter
{
	using magic_enum::istream_operators::operator>>;
	using magic_enum::ostream_operators::operator<<;
}

using magic_enum::istream_operators::operator>>;
using magic_enum::ostream_operators::operator<<;

enum class output_type { plain_text, html, csv, metadata };
enum class embed_prefix_type { none, query, passage };
template<typename T>
std::string enum_names_str()
{
	auto names = magic_enum::enum_names<T>();
	std::string names_str;
	for (auto name: names)
	{
		if (!names_str.empty())
			names_str += '|';
		names_str += name;
	}
	return names_str;
}
#ifdef DF0_LOCAL_AI
template<typename auditor_type>
static std::shared_ptr<ai::ai_runner>
create_local_runner(
    const boost::program_options::variables_map& vm,
    [[maybe_unused]] audit::auditor_ref<auditor_type> auditor)
{
    if (vm.count("local-ai-model"))
    {
        std::string model_path = vm["local-ai-model"].as<std::string>();
        if (model_path.ends_with(".gguf"))
        {
            #ifdef DF0_LLAMA
                ai::model_inference_config config;
                config.model_path = model_path;
                config.n_ctx = ai::context_size{4096};
                config.n_threads = ai::thread_count{4};
                return std::make_shared<ai::llama::llama_runner<auditor_type>>(
                    config,
                    auditor);
            #else
                throw std::runtime_error("GGUF model support requires the local-ai-llama feature");
            #endif
        }
        #ifdef DF0_CT2
            return std::make_shared<ai::ct2::ct2_runner<auditor_type>>(
                model_path,
                auditor);
        #else
            throw std::runtime_error("CT2 model support requires the local-ai-ct2 feature");
        #endif
    }

    #ifdef DF0_CT2
        return ai::local::ct2::make_default_runner(auditor);
    #elif defined(DF0_LLAMA) && defined(DF0_GRANITE)
        return ai::local::llama::make_default_runner(auditor);
    #elif defined(DF0_LLAMA)
        throw std::runtime_error(
            "No default local AI model available for this build. "
            "Specify --local-ai-model, or rebuild with the local-ai-model-granite "
            "feature to get a built-in default.");
    #else
        throw std::runtime_error(
            "Default local AI model requires the local-ai-ct2 feature or local-ai-llama feature, "
            "which can be coupled with local-ai-model-granite feature.");
    #endif
}
#endif // DF0_LOCAL_AI

namespace
{

/**
 * @brief Builds a variant of an optional pipeline step and an inert noop step.
 *
 * Returns a `std::variant` holding either the produced step (when `condition`
 * is true) or a `noop_transformer` otherwise, keeping the pipeline type stable.
 */
template <typename Factory>
auto maybe(bool condition, Factory&& factory)
{
    using T = std::decay_t<std::invoke_result_t<Factory>>;
    using V = std::variant<pipeline::noop_transformer, T>;

    if (condition)
        return V{std::invoke(std::forward<Factory>(factory))};

    return V{pipeline::noop_transformer{}};
}

/**
 * @brief Feeds the given data source into a statically typed pipeline and
 *        routes its final output to std::cout.
 */
template <typename Pipeline>
void run_pipeline(data_source&& data, Pipeline&& pipeline)
{
    pipeline::input_element{std::move(data)}
        | std::forward<Pipeline>(pipeline)
        | pipeline::output_element{std::cout};
}

} // namespace

template<typename auditor_type>
int run_with_auditor(
	const boost::program_options::variables_map& vm,
	data_source data,
	audit::auditor_ref<auditor_type> auditor);

static bool check_required_options(
	const boost::program_options::variables_map& vm,
	const char* feature,
	std::initializer_list<const char*> required,
	const char* where_to_look)
{
	if (!vm.count(feature))
		return true;
	std::string missing;
	for (const char* option : required)
	{
		if (vm.count(option))
			continue;
		if (!missing.empty())
			missing += " and ";
		missing += std::string{"--"} + option;
	}
	if (missing.empty())
		return true;
	std::cerr << "Error: --" << feature << " requires " << missing << ".\n"
	          << where_to_look << std::endl;
	return false;
}

int main(int argc, char* argv[])
{
	bool local_processing;
	bool use_stream;

	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display help message")
		("version", "display DF-0 version")
		("input-file", po::value<std::string>()->required(), "path to file to process")
		("output_type", po::value<output_type>()->default_value(output_type::plain_text), enum_names_str<output_type>().c_str())
		("http-post", po::value<std::string>(), "url to process data via http post")
		("local-ai-prompt", po::value<std::string>(), "prompt to process text via local AI model")
		("local-ai-ct2-embed", po::value<embed_prefix_type>()->implicit_value(embed_prefix_type::none), "generate embedding of text via local AI model. Optional argument selects the prefix type: (e.g. \"passage: \" or \"query: \" or \"none: \").")
		("local-ai-model", po::value<std::string>(), "path to local AI model data (build-in default model is used if not specified)")
		("openrouter-chat", po::value<std::string>(), "prompt to process text and images via OpenRouter")
		("openrouter-extract-entities", "extract entities from text and images via OpenRouter")
		("openrouter-extract-keywords", po::value<unsigned int>(), "extract N keywords/key phrases from text and images via OpenRouter")
		("openrouter-summarize", "summarize text and images via OpenRouter")
		("openrouter-detect-sentiment", "detect sentiment of text and images via OpenRouter")
		("openrouter-analyze-data", "analyze text and images for important insights and generate conclusions via OpenRouter")
		("openrouter-classify", po::value<std::vector<std::string>>()->multitoken(), "classify text and images via OpenRouter to one of specified categories")
		("openrouter-translate-to", po::value<std::string>(), "language to translate text and images to via OpenRouter")
		("openrouter-find", po::value<std::string>(), "find phrase, object or event in text and images via OpenRouter")
		("openrouter-text-to-speech", "convert text to speech via OpenRouter")
		("openrouter-embed", "generate embedding of text via OpenRouter")
		("openrouter-transcribe", "convert speech to text (transcribe) via OpenRouter")
		("openrouter-key", po::value<std::string>()->default_value(""), "OpenRouter API key (create one at https://openrouter.ai/keys)")
		("openrouter-model", po::value<std::string>()->default_value("openrouter/auto"), "OpenRouter model slug for chat-based options, e.g. openai/gpt-5 or anthropic/claude-sonnet-5 (openrouter/auto lets OpenRouter choose)")
		("openrouter-tts-model", po::value<std::string>(), "OpenRouter model slug for text-to-speech (required with --openrouter-text-to-speech), e.g. x-ai/grok-voice-tts-1.0")
		("openrouter-tts-response-format", po::value<std::string>()->default_value("mp3"), "audio format produced by text-to-speech, e.g. mp3|pcm")
		("openrouter-embed-model", po::value<std::string>(), "OpenRouter model slug for embeddings (required with --openrouter-embed), e.g. openai/text-embedding-3-small")
		("openrouter-transcribe-model", po::value<std::string>(), "OpenRouter model slug for transcription (required with --openrouter-transcribe), e.g. openai/whisper-large-v3")
		("openrouter-transcribe-language", po::value<std::string>()->default_value(""), "ISO-639-1 language code of the audio for transcription, e.g. en (auto-detect if empty)")
		("openrouter-voice", po::value<std::string>(), "voice used by text-to-speech (required with --openrouter-text-to-speech; voices are model-specific)")
		("openrouter-temperature", po::value<float>(), "force specified temperature for OpenRouter prompts")
		("openrouter-image-detail", po::value<openrouter::image_detail>()->default_value(openrouter::image_detail::automatic), enum_names_str<openrouter::image_detail>().c_str())
		("language", po::value<std::vector<language>>()->default_value({language::eng}, "eng"), "Set the document language(s) for OCR as ISO 639-3 identifiers like: spa, fra, deu, rus, chi_sim, chi_tra etc. More than 100 languages are supported. Multiple languages can be enabled.")
		("ocr-confidence-threshold", po::value<float>()->notifier([](float val) {
			if (val < 0.0f || val > 100.0f) {
				throw po::validation_error(po::validation_error::invalid_option_value,
					"ocr-confidence-threshold", std::to_string(val));
			}
		}), "Set the OCR confidence threshold (0-100). Words with confidence below this will be excluded.")
		("local-processing", po::value<bool>(&local_processing)->default_value(true), "process documents locally including OCR")
		("use-stream", po::value<bool>(&use_stream)->default_value(false), "pass file stream to SDK instead of filename")
		("min_creation_time", po::value<unsigned int>(), "filter emails by min creation time")
		("max_creation_time", po::value<unsigned int>(), "filter emails by max creation time")
		("max_nodes_number", po::value<unsigned int>(), "filter by max number of nodes")
		("folder_name", po::value<std::string>(), "filter emails by folder name")
		("attachment_extension", po::value<std::string>(), "filter by attachment type")
		("log_file", po::value<std::string>(), "set path for JSON audit output")
		("log-filter", po::value<std::string>(), "Set a custom audit filter. Filters are comma-separated and can include tags, function names such as @func:my_func, and file names such as @file:*_parser.cpp. Prepend '-' to exclude.")
		("verbose,v", "Enable verbose audit output (equivalent to --log-filter='*').")
	;

	po::positional_options_description pos_desc;
	pos_desc.add("input-file", -1);

	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	if (vm.count("help"))
	{
		readme();
		std::cout << std::endl << "Usage: df0 [options] file_name" << std::endl << std::endl << desc << std::endl;
		return 0;
	}

	if (vm.count("version"))
	{
		version();
		return 0;
	}

	try
	{
		po::notify(vm);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	if (!check_required_options(vm, "openrouter-text-to-speech",
			{"openrouter-tts-model", "openrouter-voice"},
			"List speech models and their voices (supported_voices) with:\n"
			"  curl \"https://openrouter.ai/api/v1/models?output_modalities=speech\"")
		|| !check_required_options(vm, "openrouter-embed",
			{"openrouter-embed-model"},
			"See embedding models at https://openrouter.ai/collections/embedding-models")
		|| !check_required_options(vm, "openrouter-transcribe",
			{"openrouter-transcribe-model"},
			"See transcription models at https://openrouter.ai/collections/speech-to-text-models"))
		return 1;

	std::string file_name = vm["input-file"].as<std::string>();

	data_source data = [&]() -> data_source
	{
		if (use_stream)
			return data_source{seekable_stream_ptr{
				std::make_shared<std::ifstream>(file_name, std::ios_base::binary)}};
		return data_source{std::filesystem::path{file_name}};
	}();

	#ifndef DF0_CT2
	if (vm.count("local-ai-prompt") || vm.count("local-ai-ct2-embed"))
	{
		std::cerr << "Error: Local AI features requested, but this build does not include "
		             "DF0_CT2 support.\n"
		             "Rebuild with DF0_CT2 enabled to use --local-ai-prompt or "
		             "--local-ai-ct2-embed." << std::endl;
		return 1;
	}
	#endif

	const bool audit_enabled =
		vm.count("log_file") ||
		vm.count("log-filter") ||
		vm.count("verbose");

	if (audit_enabled)
	{
		df0::ref_or_owned<std::ostream> audit_stream{std::clog};
		std::unique_ptr<std::ofstream> log_file;

		if (vm.count("log_file"))
		{
			log_file = std::make_unique<std::ofstream>(vm["log_file"].as<std::string>());
			if (!log_file->is_open())
			{
				std::cerr << "Error: Unable to open log file: " << vm["log_file"].as<std::string>() << std::endl;
				return 1;
			}
			audit_stream = df0::ref_or_owned<std::ostream>{std::move(*log_file)};
		}

		const std::string filter_spec =
			vm.count("log-filter")
				? vm["log-filter"].as<std::string>()
				: vm.count("verbose")
					? "*"
					: "";

		df0::audit::json_stream_auditor auditor{
			filter_spec,
			std::move(audit_stream)
		};

		return run_with_auditor(
			vm,
			std::move(data),
			df0::audit::auditor_ref<df0::audit::json_stream_auditor>{auditor});
	}

	df0::audit::null_auditor auditor;
	return run_with_auditor(
		vm,
		std::move(data),
		df0::audit::auditor_ref<df0::audit::null_auditor>{auditor});
}

template<typename auditor_type>
int run_with_auditor(
	const boost::program_options::variables_map& vm,
	data_source data,
	audit::auditor_ref<auditor_type> auditor)
{
	using configured_auditor = auditor_type;

	std::string file_name = vm["input-file"].as<std::string>();
	bool local_processing = vm["local-processing"].as<bool>();

	audit::entry(auditor, [&]{
		return "use_stream"_v = vm["use-stream"].as<bool>(), "file_name"_v = file_name;
	});

	const std::string& key = vm["openrouter-key"].as<std::string>();
	const std::string& model = vm["openrouter-model"].as<std::string>();
	const float temperature =
		vm.count("openrouter-temperature")
			? vm["openrouter-temperature"].as<float>()
			: 0.0f;
	const openrouter::image_detail image_detail =
		vm["openrouter-image-detail"].as<openrouter::image_detail>();

	auto optional_suffix =
		maybe(vm.count("http-post"), [&] {
			return http::post<configured_auditor>(
				vm["http-post"].as<std::string>(),
				"",
				http::ssl_verify_peer{true},
				auditor);
		})
		| maybe(vm.count("openrouter-chat"), [&] {
			return openrouter::task<configured_auditor>(
				vm["openrouter-chat"].as<std::string>(),
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-extract-entities"), [&] {
			return openrouter::extract_entities<configured_auditor>(
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-extract-keywords"), [&] {
			return openrouter::extract_keywords<configured_auditor>(
				vm["openrouter-extract-keywords"].as<unsigned int>(),
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-summarize"), [&] {
			return openrouter::summarize<configured_auditor>(
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-detect-sentiment"), [&] {
			return openrouter::detect_sentiment<configured_auditor>(
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-analyze-data"), [&] {
			return openrouter::analyze_data<configured_auditor>(
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-classify"), [&] {
			const std::vector<std::string>& categories =
				vm["openrouter-classify"].as<std::vector<std::string>>();
			std::set<std::string> categories_set(categories.begin(), categories.end());
			return openrouter::classify<configured_auditor>(
				categories_set,
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-translate-to"), [&] {
			return openrouter::translate_to<configured_auditor>(
				vm["openrouter-translate-to"].as<std::string>(),
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-find"), [&] {
			return openrouter::find<configured_auditor>(
				vm["openrouter-find"].as<std::string>(),
				key,
				model,
				temperature,
				image_detail,
				auditor);
		})
		| maybe(vm.count("openrouter-text-to-speech"), [&] {
			return openrouter::text_to_speech<configured_auditor>(
				key,
				vm["openrouter-tts-model"].as<std::string>(),
				vm["openrouter-voice"].as<std::string>(),
				vm["openrouter-tts-response-format"].as<std::string>(),
				auditor);
		})
		| maybe(vm.count("openrouter-embed"), [&] {
			return openrouter::embed<configured_auditor>(
				key,
				vm["openrouter-embed-model"].as<std::string>(),
				auditor);
		})
#ifdef DF0_LOCAL_AI
		| maybe(vm.count("local-ai-prompt"), [&] {
			auto runner = create_local_runner(
				vm,
				auditor);
			return ai::local::task(
				vm["local-ai-prompt"].as<std::string>(), runner);
		})
#endif
#ifdef DF0_CT2
		| maybe(vm.count("local-ai-ct2-embed") &&
			vm["local-ai-ct2-embed"].as<embed_prefix_type>() == embed_prefix_type::query, [&] {
			return ai::local::ct2::query::embedder<configured_auditor>{
				auditor
			};
		})
		| maybe(vm.count("local-ai-ct2-embed") &&
					   vm["local-ai-ct2-embed"].as<embed_prefix_type>() == embed_prefix_type::passage, [&] {
			return ai::local::ct2::passage::embedder<configured_auditor>{
				auditor
			};
		})
		| maybe(vm.count("local-ai-ct2-embed") &&
					   vm["local-ai-ct2-embed"].as<embed_prefix_type>() == embed_prefix_type::none, [&] {
			return ai::embed<configured_auditor>{
				ai::local::ct2::make_embedding_runner(auditor),
				std::string{},
				auditor
			};
		})
#endif
		| pipeline::function_transformer{[](message_ptr msg,
			 const message_callbacks& emit_message) -> continuation
			{
				if (msg->is<ai::embedding>())
				{
					const auto& embedding_vec = msg->get<ai::embedding>().values;
					std::string embedding_str = "[";
					for (size_t i = 0; i < embedding_vec.size(); ++i)
					{
						embedding_str += std::to_string(embedding_vec[i]);
						if (i < embedding_vec.size() - 1)
							embedding_str += ", ";
					}
					embedding_str += "]";
					return emit_message(data_source{embedding_str});
				}
				return emit_message(std::move(msg));
			}}
		| pipeline::function_transformer{[](message_ptr msg,
			 const message_callbacks& emit_message) -> continuation
			{
				if (msg->is<std::exception_ptr>())
					std::clog << "[WARNING] "
						<< errors::diagnostic_message(
							   msg->get<std::exception_ptr>())
						<< std::endl;
				return emit_message(std::move(msg));
			}};

	if (vm.count("openrouter-transcribe"))
	{
		auto pipeline =
			content_type::detector{}
			| openrouter::transcribe<configured_auditor>(
				key,
				vm["openrouter-transcribe-model"].as<std::string>(),
				vm["openrouter-transcribe-language"].as<std::string>(),
				auditor)
			| plain_text_exporter{}
			| std::move(optional_suffix);

		try
		{
			run_pipeline(std::move(data), pipeline);
		}
		catch (const std::exception& e)
		{
			std::cerr << "[ERROR] " << errors::diagnostic_message(e)
				<< "processing file " + file_name << std::endl;
			return 2;
		}
		catch (...)
		{
			std::cerr << "[ERROR] Unknown error\nprocessing file " + file_name << std::endl;
			return 2;
		}
		return 0;
	}

	if (local_processing)
	{
		ocr_confidence_threshold threshold_arg{};
		if (vm.count("ocr-confidence-threshold"))
			threshold_arg.v = vm["ocr-confidence-threshold"].as<float>();

		using exporter_variant =
			std::variant<plain_text_exporter,
			             html_exporter<configured_auditor>,
			             csv_exporter,
			             metadata_exporter<configured_auditor>>;

		exporter_variant formatter = plain_text_exporter{};

		switch (vm["output_type"].as<output_type>())
		{
			case output_type::plain_text:
				formatter = plain_text_exporter{};
				break;
			case output_type::html:
				formatter = html_exporter<configured_auditor>{
					auditor
				};
				break;
			case output_type::csv:
				formatter = csv_exporter{};
				break;
			case output_type::metadata:
				formatter = metadata_exporter<configured_auditor>{
					auditor
				};
				break;
		}

		auto local_prefix =
			content_type::detector{}
			| archives_parser<configured_auditor>{
				auditor}
			| office_formats_parser<df0::safety_policy::strict, configured_auditor>{
				auditor}
			| mail_parser<configured_auditor>{
				auditor}
			| ocr_parser<configured_auditor>{
				vm["language"].as<std::vector<language>>(),
				threshold_arg,
				{},
				{},
				auditor}
			| maybe(vm.count("max_nodes_number"), [&] {
				  return standard_filter::filterByMaxNodeNumber(
					  vm["max_nodes_number"].as<unsigned int>());
			  })
			| maybe(vm.count("min_creation_time"), [&] {
				  return standard_filter::filterByMailMinCreationTime(
					  vm["min_creation_time"].as<unsigned int>());
			  })
			| maybe(vm.count("max_creation_time"), [&] {
				  return standard_filter::filterByMailMaxCreationTime(
					  vm["max_creation_time"].as<unsigned int>());
			  })
			| maybe(vm.count("folder_name"), [&] {
				  return standard_filter::filterByFolderName(
					  {vm["folder_name"].as<std::string>()});
			  })
			| maybe(vm.count("attachment_extension"), [&] {
				  return standard_filter::filterByAttachmentType(
					  {file_extension{
						  vm["attachment_extension"].as<std::string>()}});
			  });

		auto pipeline =
			std::move(local_prefix)
			| std::move(formatter)
			| std::move(optional_suffix);

		try
		{
			run_pipeline(std::move(data), pipeline);
		}
		catch (const std::exception& e)
		{
			std::cerr << "[ERROR] " << errors::diagnostic_message(e)
				<< "processing file " + file_name << std::endl;
			return 2;
		}
		catch (...)
		{
			std::cerr << "[ERROR] Unknown error\nprocessing file " + file_name << std::endl;
			return 2;
		}
		return 0;
	}

	auto pipeline =
		content_type::detector{}
		| std::move(optional_suffix);

	try
	{
		run_pipeline(std::move(data), pipeline);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[ERROR] " << errors::diagnostic_message(e)
			<< "processing file " + file_name << std::endl;
		return 2;
	}
	catch (...)
	{
		std::cerr << "[ERROR] Unknown error\nprocessing file " + file_name << std::endl;
		return 2;
	}

	return 0;
}
