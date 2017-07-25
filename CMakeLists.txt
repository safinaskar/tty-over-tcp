cmake_minimum_required (VERSION 3.1) # 3.1 adds C_STANDARD
project (tty-over-tcp LANGUAGES C)
add_executable (tty-over-tcp tty-over-tcp.c tcp.c)
set_property (TARGET tty-over-tcp PROPERTY C_STANDARD 99)
target_link_libraries (tty-over-tcp -lsh -lutil)
install (TARGETS tty-over-tcp DESTINATION bin)
