############################################################################
#
#   Copyright (c) 2026 PX4 Development Team. All rights reserved.
#
############################################################################

# i.MX 8M Plus Cortex-M7 (NuttX custom chip "mx8mp")
add_subdirectory(../imxrt/board_critmon board_critmon)
add_subdirectory(board_reset)
add_subdirectory(hrt)
add_subdirectory(version)
