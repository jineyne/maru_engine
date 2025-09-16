project(glad)
add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)
target_compile_features(glad PUBLIC c_std_99)