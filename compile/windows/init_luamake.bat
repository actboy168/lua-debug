if exist luamake\.git (

    pushd luamake

    git pull
    git submodule update --remote

    popd

) else (

    git clone https://github.com/actboy168/luamake.git --recurse-submodules

)
