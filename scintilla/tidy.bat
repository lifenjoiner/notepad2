@clang-tidy %* -- --target=x86_64-pc-windows-msvc -std=c++20 -O2 -fno-rtti -D_WIN64 -DNOMINMAX -DNDEBUG -DWIN32_LEAN_AND_MEAN -DSTRICT_TYPED_ITEMIDS -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -DBOOST_REGEX_STANDALONE -DNO_CXX11_REGEX -Iinclude -Isrc -Ilexlib -Wall -Wextra -Wshadow -Wimplicit-fallthrough -Wformat=2 -Wundef -Wcomma -Wold-style-cast 1>tidy.log
@rem @clang-tidy %* -- --target=x86_64-pc-windows-msvc -march=x86-64-v3 -std=c++20 -O2 -fno-rtti -D_WIN64 -DNOMINMAX -DNDEBUG -DWIN32_LEAN_AND_MEAN -DSTRICT_TYPED_ITEMIDS -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -DBOOST_REGEX_STANDALONE -DNO_CXX11_REGEX -Iinclude -Isrc -Ilexlib -Wall -Wextra -Wshadow -Wimplicit-fallthrough -Wformat=2 -Wundef -Wcomma -Wold-style-cast 1>tidy.log
