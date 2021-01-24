set(srcs
    "main.c"
    "gatt_svr.c"
    "segments.c"
    "ui.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS ".")
