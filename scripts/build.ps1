param([switch]$Test)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    if (Get-Command g++ -ErrorAction SilentlyContinue) {
        & g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Ithird_party -Iserver server/main.cpp -o build/minididi.exe -lws2_32 -lcrypt32 -static-libgcc -static-libstdc++
        if ($LASTEXITCODE -ne 0) { throw 'Server build failed' }
        if ($Test) {
            & g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ithird_party -Iserver tests/core_tests.cpp -o build/core_tests.exe -static-libgcc -static-libstdc++
            if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
            & .\build\core_tests.exe
            if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
        }
    } elseif (Get-Command cmake -ErrorAction SilentlyContinue) {
        & cmake -S . -B build
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
        & cmake --build build --config Release
        if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
        if ($Test) { & ctest --test-dir build -C Release --output-on-failure; if ($LASTEXITCODE -ne 0) { throw 'Tests failed' } }
    } else { throw 'Install MinGW-w64 g++ or Visual Studio C++ and CMake.' }
} finally { Pop-Location }
