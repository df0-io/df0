/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_DF0_H
#define DF0_DF0_H

// IWYU pragma: begin_exports
#include "ai_runner.h"
#include "ai_detect_sentiment.h"
#include "ai_summarize.h"
#include "ai_translate.h"
#include "ai_embed.h"
#include "ai_find.h"
#include "ai_task.h"
#include "model_chain_element.h"
#ifdef DF0_CT2
#include "ct2_runner.h"
#include "local_ai_ct2_embed.h"
#include "local_ai_ct2_find.h"
#include "local_ai_ct2_runner_factory.h"
#include "local_ai_ct2_summarize.h"
#include "local_ai_ct2_translate.h"
#include "local_ai_ct2_task.h"
#include "local_ai_ct2_detect_sentiment.h"
#endif
#ifdef DF0_LLAMA
#include "llama_runner.h"
#include "model_inference_config.h"
#include "llama_models.h"
#include "local_ai_llama_runner_factory.h"
#include "local_ai_llama_find.h"
#include "local_ai_llama_summarize.h"
#include "local_ai_llama_task.h"
#include "local_ai_llama_translate.h"
#include "local_ai_llama_detect_sentiment.h"
#endif
#ifdef DF0_LOCAL_AI
#include "local_ai_find.h"
#include "local_ai_summarize.h"
#include "local_ai_translate.h"
#include "local_ai_task.h"
#include "local_ai_detect_sentiment.h"
#endif
#include "ai_elements.h"

#include "concepts.h"
#include "content_type.h"
#include "content_type_by_file_extension.h"
#include "content_type_html.h"
#include "content_type_iwork.h"
#include "content_type_odf_flat.h"
#include "content_type_outlook.h"
#include "content_type_xlsb.h"
#include "convert.h"
#include "cosine_similarity.h"
#include "archives_parser.h"
#include "ensure.h"
#include "environment.h"
#include "fuzzy_match.h"
#include "pipeline.hpp"
#include "mail_elements.h"
#include "mail_parser.h"
#include "ocr_parser.h"
#include "office_formats_parser.h"
#include "plain_text_exporter.h"
#include "plain_text_writer.h"
#include "html_exporter.h"
#include "serialization.h"
#include "type_name.h"
#include "xml.h"
#include "openrouter_analyze_data.h"
#include "openrouter_classify.h"
#include "openrouter_detect_sentiment.h"
#include "openrouter_embed.h"
#include "openrouter_extract_entities.h"
#include "openrouter_extract_keywords.h"
#include "openrouter_find.h"
#include "openrouter_summarize.h"
#include "openrouter_task.h"
#include "openrouter_transcribe.h"
#include "openrouter_translate_to.h"
#include "openrouter_text_to_speech.h"
// IWYU pragma: end_exports

/**
 * @example integration_example.cmake
 * This example presents how to integrate DF-0 with your project using CMake
*/

/**
 * @example handling_errors_and_warnings.cpp
 * This example presents how to catch exceptions to handle critical errors and
 * how to use transformer to process non-critical errors (warnings) flowing through the pipeline
*/

/**
 * @example file_type_determination.cpp
 * This example shows how to perform file type detection (with or without document processing)
 * by file extension or file signature or rely on provided mime-type
*/

/**
 * @example path_to_text_stream.cpp
 * This example shows how to parse a document from a file path and export its plain text content to a stream.
*/

/**
 * @example stream_to_html.cpp
 * This example shows how to parse a document from a stream and export its content to HTML.
*/

/**
 * @example parse_archives.cpp
 * This example demonstrates how to recursively parse files contained within an archive (e.g., ZIP, TAR).
*/

/**
 * @example local_ai_classify.cpp
 * This example shows how to classify a document into one of several categories using a built-in local AI model.
*/

/**
 * @example openrouter_classify.cpp
 * This example shows how to classify a document using the OpenRouter API.
*/

/**
 * @example local_ai_translate.cpp
 * This example demonstrates how to translate a document to another language using a built-in local AI model.
*/

/**
 * @example openrouter_translate.cpp
 * This example shows how to translate a document using the OpenRouter API.
*/

/**
 * @example local_ai_sentiment.cpp
 * This example shows how to detect the sentiment of a document using a built-in local AI model.
*/

/**
 * @example openrouter_sentiment.cpp
 * This example shows how to detect the sentiment of a document using the OpenRouter API.
*/

/**
 * @example local_ai_summary.cpp
 * This example demonstrates how to create a summary of a document using a built-in local AI model.
*/

/**
 * @example openrouter_transcribe.cpp
 * This example shows how to transcribe an audio file to text using the OpenRouter API.
*/

/**
 * @example openrouter_text_to_speech.cpp
 * This example shows how to convert text to speech (an mp3 audio stream) using the OpenRouter API.
*/

/**
 * @example local_ai_find.cpp
 * This example demonstrates how to find a specific sentence in a document using a built-in local AI model.
*/

/**
 * @example openrouter_find_image.cpp
 * This example shows how to find objects within an image using the OpenRouter API.
*/

/**
 * @example openrouter_embedding.cpp
 * This example shows how to generate a text embedding for a document using the OpenRouter API.
*/

/**
 * @example local_embedding_similarity.cpp
 * This example demonstrates how to create embeddings for a document and queries using a local AI model and then calculate their similarity.
*/

/**
 * @example reuse_chain.cpp
 * This example shows how to create a single processing chain and reuse it to parse multiple input files.
*/

/**
 * @example filter_emails_by_subject.cpp
 * This example shows how to use a transformer to filter emails from a PST file based on their subject line.
*/

/**
 * @example join_transformers.cpp
 * This example demonstrates how to join multiple transformers to create a more complex filter.
*/

/**
 * @example xml_parsing_example.cpp
 * This example demonstrates how to parse XML documents using the DF-0 modern C++ API.
*/

/**
 * @namespace df0
 * @brief The main namespace for the DF-0.
 */

#endif
