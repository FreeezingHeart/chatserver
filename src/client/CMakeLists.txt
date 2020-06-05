aux_source_directory(. SRC_LIST)

add_executable(ChatClient ${SRC_LIST})
target_link_libraries(ChatClient pthread)