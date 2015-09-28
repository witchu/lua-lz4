@rd /s/q c:\work c:\luarocks 2> nul
set
pushd c:\
md \work
cd \work
powershell -command "wget 'http://www2.cs.uidaho.edu/~jeffery/win32/unzip.exe' -OutFile 'unzip.exe'"
powershell -command "wget 'http://keplerproject.github.io/luarocks/releases/luarocks-2.2.2-win32.zip' -OutFile 'luarocks.zip'"
unzip luarocks.zip
cd luarocks-2.2.2-win32
call install /P c:\luarocks /SELFCONTAINED /L /NOADMIN /F /Q
call c:\luarocks\2.2\luarocks path --bin > setpath.bat
call setpath.bat
set lua_path=.\?.lua;%luapath%
popd
