file(GLOB_RECURSE SOURCES "src/*.c")
file(GLOB_RECURSE WEB_SOURCES "web_pages/*")

idf_component_register(
    SRCS  
         ${SOURCES}
        "main.c"
    INCLUDE_DIRS 
        "."
        "include"
    EMBED_FILES
        ${WEB_SOURCES}
)
