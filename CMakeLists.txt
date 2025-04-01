cmake_minimum_required(VERSION 3.10)
project(trie)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(/workspaces/vpp/build-root/install-vpp_debug-native/vpp/include/)
link_directories(/workspaces/vpp/build-root/install-vpp_debug-native/vpp/lib/aarch64-linux-gnu/)

add_executable(trie main.c domain_trie.c iprtree.c domain_iprtree.c)

target_link_libraries(trie vppinfra)
