@echo off
PATH=%PATH%;C:\Program Files\LLVM\bin
clang --target=x86_64-pc-windows-msvc -DIPHLPMOD_EXPORTS -O2 -c iphlpmod.c -o iphlpmod.obj
lld-link /dll /entry:DllMain /nodefaultlib /machine:x64 iphlpmod.obj
exit
