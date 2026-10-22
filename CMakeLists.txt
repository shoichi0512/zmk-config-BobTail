# SPDX-License-Identifier: MIT
add_subdirectory(drivers/pmw3610)
zephyr_library()
zephyr_library_include_directories(include)
zephyr_library_sources(src/behavior_scroll_speed.c)
zephyr_library_sources_ifdef(CONFIG_BOBTAIL_KEEP_HFXO_ON src/hfxo_hold.c)
