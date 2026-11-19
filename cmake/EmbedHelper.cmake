# The daemon embeds the exact helper built alongside it; no installed helper
# path or mutable launcher participates in parallel authentication.
if(NOT DEFINED HELPER OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "HELPER and OUTPUT are required")
endif()
file(TO_CMAKE_PATH "${HELPER}" helper_path)
file(WRITE "${OUTPUT}" "// Generated from the paired sddm-helper executable.\n__asm__(\".pushsection .rodata\\n\"\n        \".balign 16\\n\"\n        \".global sddm_parallel_helper_begin\\n\"\n        \"sddm_parallel_helper_begin:\\n\"\n        \".incbin \\\"${helper_path}\\\"\\n\"\n        \".global sddm_parallel_helper_end\\n\"\n        \"sddm_parallel_helper_end:\\n\"\n        \".popsection\\n\");\n")
