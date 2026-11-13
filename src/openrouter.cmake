add_library(df0_openrouter SHARED
    openrouter_task.cpp)

find_package(Boost REQUIRED COMPONENTS json)
target_link_libraries(df0_openrouter PUBLIC df0_base64 df0_core df0_ai df0_http Boost::json)

install(TARGETS df0_openrouter EXPORT df0_targets)
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:df0_openrouter> DESTINATION bin CONFIGURATIONS Debug)
endif()

include(GenerateExportHeader)
generate_export_header(df0_openrouter EXPORT_FILE_NAME openrouter_export.h)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/openrouter_export.h DESTINATION include/df0)
