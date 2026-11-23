############################################################################
#
#   Copyright (c) 2026 PX4 Development Team. All rights reserved.
#
############################################################################

add_library(drivers_board
	init.c
	rpmsg_tty.c
)

target_link_libraries(drivers_board
	PRIVATE
		nuttx_arch
		nuttx_drivers
		px4_layer
)

# Board replaces NuttX's audio-oriented mx8mp_clockconfig() (see init.c)
target_link_libraries(drivers_board INTERFACE -Wl,--wrap=mx8mp_clockconfig)
