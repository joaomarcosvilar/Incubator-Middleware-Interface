idf_component_register(
    SRCS 
        "main.c"
        "application/application.c"
        "espnow_manager/espnow_manager.c"
        "files/fs_manager.c"
        
    INCLUDE_DIRS   
        "."
        "application"
        "files"            
        "espnow_manager"
)
