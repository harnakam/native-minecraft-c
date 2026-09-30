param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$repoDir = Split-Path -Parent $PSScriptRoot
$ucrtBin = 'C:\msys64\ucrt64\bin'
if (-not (Test-Path -LiteralPath "$ucrtBin\gcc.exe")) {
    throw 'Install MSYS2 UCRT64 gcc, cmake, ninja and zlib; see README.md.'
}
$env:Path = "$ucrtBin;" + $env:Path
& "$ucrtBin\cmake.exe" -S $repoDir -B "$repoDir\build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$ucrtBin\gcc.exe" '-DCMAKE_PREFIX_PATH=C:\msys64\ucrt64'
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& "$ucrtBin\cmake.exe" --build "$repoDir\build"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if (-not $SkipTests) {
    & "$ucrtBin\ctest.exe" --test-dir "$repoDir\build" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
}
