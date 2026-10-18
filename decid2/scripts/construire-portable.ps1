# Decid 2 - construction de Decid.exe portable (un seul fichier, sans installation).
# Slint et OpenCV sont compiles en bibliotheques statiques depuis leurs sources officielles, avec le CRT statique /MT ;
# Decid.exe contient l'interface et le moteur ; les jeux databasesCSV y sont embarques.
#
# Prerequis : Visual Studio 2022 (charge de travail C++), CMake, Rust (cargo, pour Slint), et les sources :
#   - Slint 1.18.1 : https://github.com/slint-ui/slint/archive/refs/tags/v1.18.1.zip
#   - OpenCV 4.10.0 : https://github.com/opencv/opencv/archive/refs/tags/4.10.0.zip
# ainsi que Slint 1.18.1 precompile (installeur Slint-cpp-1.18.1-win64-MSVC-AMD64.exe) pour son compilateur .slint.
#
# Exemple :
#   powershell -ExecutionPolicy Bypass -File scripts\construire-portable.ps1 `
#     -SourcesSlint C:\src\slint-1.18.1 -SourcesOpenCV C:\src\opencv-4.10.0 -SlintPrecompile C:\Slint
# Resultat : build-portable\app\Release\Decid.exe
param(
    [Parameter(Mandatory = $true)][string]$SourcesSlint,
    [Parameter(Mandatory = $true)][string]$SourcesOpenCV,
    [Parameter(Mandatory = $true)][string]$SlintPrecompile,
    [string]$Bibliotheque = ""
)
$ErrorActionPreference = "Stop"
$racine = Split-Path -Parent $PSScriptRoot                 # dossier decid2
$build = Join-Path $racine "build-portable"
$slintStatique = Join-Path $racine "externe\slint-statique"
$opencvStatique = Join-Path $racine "externe\opencv-statique"

function Etape([string]$nom, [scriptblock]$action) {
    Write-Host "== $nom"
    & $action
    if ($LASTEXITCODE -ne 0) { throw "$nom : echec (code $LASTEXITCODE)" }
}

# 1. Slint statique (Rust : CRT statique aussi)
if (-not (Test-Path (Join-Path $slintStatique "lib\cmake"))) {
    $env:RUSTFLAGS = "-Ctarget-feature=+crt-static"
    $b = Join-Path $build "_slint"
    Etape "Slint : configuration" { cmake -S $SourcesSlint -B $b -G "Visual Studio 17 2022" -A x64 `
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
        "-DSLINT_COMPILER=$(Join-Path $SlintPrecompile 'bin\slint-compiler.exe')" `
        -DSLINT_FEATURE_INTERPRETER=OFF -DSLINT_FEATURE_LIVE_PREVIEW=OFF -DSLINT_FEATURE_TESTING=OFF `
        -DSLINT_FEATURE_RENDERER_SKIA=OFF -DSLINT_FEATURE_BACKEND_WINIT=ON -DSLINT_FEATURE_RENDERER_FEMTOVG=ON `
        -DSLINT_FEATURE_RENDERER_SOFTWARE=ON -DSLINT_FEATURE_SYSTEM_TRAY=OFF "-DCMAKE_INSTALL_PREFIX=$slintStatique" }
    Etape "Slint : compilation" { cmake --build $b --config Release --parallel }
    Etape "Slint : installation" { cmake --install $b --config Release }
}

# 2. OpenCV statique, limite aux modules utilises par le moteur (core, imgproc, ml, imgcodecs), sans IPP
if (-not (Test-Path (Join-Path $opencvStatique "OpenCVConfig.cmake"))) {
    $b = Join-Path $build "_opencv"
    Etape "OpenCV : configuration" { cmake -S $SourcesOpenCV -B $b -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 `
        -DOPENCV_PYTHON_SKIP_DETECTION=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_WITH_STATIC_CRT=ON -DBUILD_LIST=core,imgproc,ml,imgcodecs `
        -DBUILD_opencv_world=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF -DBUILD_opencv_apps=OFF `
        -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF -DBUILD_opencv_python2=OFF -DBUILD_opencv_js=OFF `
        -DWITH_IPP=OFF -DWITH_OPENCL=OFF -DWITH_FFMPEG=OFF -DWITH_MSMF=OFF -DWITH_DSHOW=OFF -DWITH_ITT=OFF -DWITH_EIGEN=OFF `
        -DWITH_LAPACK=OFF -DWITH_PROTOBUF=OFF -DWITH_ADE=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF `
        -DWITH_OPENEXR=OFF -DWITH_AVIF=OFF -DWITH_IMGCODEC_HDR=OFF -DWITH_IMGCODEC_SUNRASTER=OFF -DWITH_IMGCODEC_PXM=OFF `
        -DWITH_IMGCODEC_PFM=OFF -DWITH_PNG=ON -DWITH_JPEG=ON -DBUILD_PNG=ON -DBUILD_JPEG=ON -DBUILD_ZLIB=ON -DWITH_VTK=OFF `
        -DWITH_GTK=OFF -DWITH_WIN32UI=OFF -DWITH_DIRECTX=OFF -DWITH_CUDA=OFF -DWITH_OPENVINO=OFF -DWITH_OBSENSOR=OFF `
        "-DCMAKE_INSTALL_PREFIX=$opencvStatique" }
    Etape "OpenCV : compilation" { cmake --build $b --config Release --parallel }
    Etape "OpenCV : installation" { cmake --install $b --config Release }
}

# 3. Decid.exe portable
$options = @("-DDECID_PORTABLE=ON")   # Slint statique trouvee dans externe\slint-statique
if ($Bibliotheque) { $options += "-DDECID_BIBLIOTHEQUE=$Bibliotheque" }
Etape "Decid : configuration" { cmake -S $racine -B $build -G "Visual Studio 17 2022" -A x64 @options }
Etape "Decid : compilation" { cmake --build $build --config Release --target decid-studio --parallel }
Write-Host "Decid.exe portable : $(Join-Path $build 'app\Release\Decid.exe')"
