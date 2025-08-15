$function:prompt = $function:_old_conan_conanenv_prompt
remove-item function:_old_conan_conanenv_prompt


$env:PATH=$env:CONAN_OLD_conanenv_PATH
Remove-Item env:CONAN_OLD_conanenv_PATH
$env:PKG_CONFIG_PATH=$env:CONAN_OLD_conanenv_PKG_CONFIG_PATH
Remove-Item env:CONAN_OLD_conanenv_PKG_CONFIG_PATH
Remove-Item env:GNUSTEP_CONAN_PACKAGE_ROOT
Remove-Item env:GNUSTEP_CONAN_LOCAL_PARENT_WINPATH
Remove-Item env:GNUSTEP_SYSTEM_ROOT
Remove-Item env:GNUSTEP_MAKEFILES
Remove-Item env:GNUSTEP_SYSTEM_APPS
Remove-Item env:GNUSTEP_SYSTEM_ADMIN_APPS
Remove-Item env:GNUSTEP_SYSTEM_WEB_APPS
Remove-Item env:GNUSTEP_SYSTEM_TOOLS
Remove-Item env:GNUSTEP_SYSTEM_ADMIN_TOOLS
Remove-Item env:GNUSTEP_SYSTEM_LIBRARY
Remove-Item env:GNUSTEP_SYSTEM_HEADERS
Remove-Item env:GNUSTEP_SYSTEM_LIBRARIES
Remove-Item env:GNUSTEP_SYSTEM_DOC
Remove-Item env:GNUSTEP_SYSTEM_DOC_MAN
Remove-Item env:GNUSTEP_SYSTEM_DOC_INFO
Remove-Item env:GNUSTEP_LOCAL_APPS
Remove-Item env:GNUSTEP_LOCAL_ADMIN_APPS
Remove-Item env:GNUSTEP_LOCAL_WEB_APPS
Remove-Item env:GNUSTEP_LOCAL_TOOLS
Remove-Item env:GNUSTEP_LOCAL_ADMIN_TOOLS
Remove-Item env:GNUSTEP_LOCAL_LIBRARY
Remove-Item env:GNUSTEP_LOCAL_HEADERS
Remove-Item env:GNUSTEP_LOCAL_LIBRARIES
Remove-Item env:GNUSTEP_LOCAL_DOC
Remove-Item env:GNUSTEP_LOCAL_DOC_MAN
Remove-Item env:GNUSTEP_LOCAL_DOC_INFO
Remove-Item env:GNUSTEP_SYSTEM_USERS_DIR
Remove-Item env:GNUSTEP_LOCAL_USERS_DIR
Remove-Item env:GNUSTEP_NETWORK_USERS_DIR
Remove-Item env:GNUSTEP_USER_CONFIG_FILE
Remove-Item env:GNUSTEP_USER_DEFAULTS_DIR
Remove-Item env:LD_LIBRARY_PATH
Remove-Item env:CLASSPATH
Remove-Item env:GUILE_LOAD_PATH