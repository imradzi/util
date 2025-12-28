file(GLOB util_SRC
    "*.cpp"
    "*.h"
)
add_library(util ${util_SRC})
SetIncludeDirectories(util)