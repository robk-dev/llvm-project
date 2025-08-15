#!/usr/bin/env sh
export CONAN_OLD_PATH="$PATH"
export CONAN_OLD_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

while read -r line; do
    LINE="$(eval echo $line)";
    export "$LINE";
done < "C:\code\llvm-project\lldb\examples\environment.sh.env"

export CONAN_OLD_PS1="$PS1"
export PS1="(conanenv) $PS1"