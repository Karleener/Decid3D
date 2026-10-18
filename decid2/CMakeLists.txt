# Decid 2 - moteur, outil en ligne de commande et worker
# Aucune dependance telechargee : OpenCV 4.10 precompile local uniquement.
cmake_minimum_required(VERSION 3.20)
# Executable portable : Slint et OpenCV statiques sont compiles en Release seulement, donc pas de configuration Debug
if(DECID_PORTABLE)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Configurations disponibles" FORCE)
endif()
project(Decid2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Executable portable (section « Executable portable ») : Slint et OpenCV
# statiques compiles depuis leurs sources (externe/slint-statique, externe/opencv-statique), CRT statique /MT,
# moteur et bibliotheque embarques dans Decid.exe. A utiliser dans un dossier de build separe (build-portable).
option(DECID_PORTABLE "Decid.exe unique et portable (Slint, OpenCV et CRT statiques, moteur embarque)" OFF)
if(DECID_PORTABLE)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(NOT OpenCV_DIR)
    set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externe/opencv-statique" CACHE PATH "Dossier OpenCVConfig.cmake")
  endif()
  set(OpenCV_STATIC ON)
endif()

# OpenCV precompile (D:/opencv/build par defaut, surchargeable)
if(NOT OpenCV_DIR)
  set(OpenCV_DIR "D:/opencv/build" CACHE PATH "Dossier OpenCVConfig.cmake")
endif()
find_package(OpenCV 4.10 EXACT REQUIRED COMPONENTS core ml imgproc imgcodecs)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
endif()

# decid_base : sans OpenCV (utilisable par l'interface, qui ne doit pas charger opencv_world)
add_library(decid_base STATIC
  noyau/Erreur.cpp
  noyau/Sha256.cpp
  noyau/Json.cpp
  noyau/Fichiers.cpp
  noyau/Donnees.cpp
  noyau/Partage.cpp
  noyau/Metriques.cpp
  noyau/Trace.cpp
  noyau/Densites.cpp
)
target_include_directories(decid_base PUBLIC noyau)

# decid_noyau : calculs OpenCV (worker et outil en ligne de commande)
add_library(decid_noyau STATIC
  noyau/Normalisation.cpp
  noyau/Modeles.cpp
  noyau/Inspection.cpp
  noyau/Experience.cpp
  noyau/Vue.cpp
  noyau/Validation.cpp
  noyau/Explicable.cpp
  noyau/Operations.cpp
  noyau/Reseau.cpp
  noyau/Scores.cpp
  noyau/Generation.cpp
  noyau/Regression.cpp
  noyau/Segmentation.cpp
  noyau/Decision.cpp
)
target_include_directories(decid_noyau PUBLIC noyau)
target_link_libraries(decid_noyau PUBLIC decid_base)
target_link_libraries(decid_noyau PUBLIC ${OpenCV_LIBS})

# Commandes du moteur : DecidMoteur.exe (ligne de commande) et Decid.exe (Decid.exe --moteur ...) les partagent
add_library(decid_outil STATIC outil/LigneCommande.cpp outil/Worker.cpp)
target_include_directories(decid_outil PUBLIC outil)
target_link_libraries(decid_outil PUBLIC decid_noyau)
add_executable(decid-moteur outil/DecidMoteur.cpp)
target_link_libraries(decid-moteur PRIVATE decid_outil)
set_target_properties(decid-moteur PROPERTIES OUTPUT_NAME DecidMoteur)

# Copie de la DLL OpenCV a cote des executables (aucune modification de la source)
set(DECID_OPENCV_DLL "${OpenCV_DIR}/x64/vc16/bin/opencv_world4100$<$<CONFIG:Debug>:d>.dll")   # DLL de debogage en Debug

enable_testing()
set(DECID_TESTS test_sha256 test_json test_donnees test_partage test_normalisation
                test_metriques test_modeles test_trace
                test_vue test_validation test_explicable test_operations
                test_familles test_densites test_reseau
                test_generation test_regression test_segmentation)
foreach(t ${DECID_TESTS})
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE decid_noyau)
  target_include_directories(${t} PRIVATE tests)
  add_test(NAME ${t} COMMAND ${t} "${CMAKE_CURRENT_SOURCE_DIR}" WORKING_DIRECTORY $<TARGET_FILE_DIR:${t}>)
endforeach()
add_test(NAME test_worker COMMAND test_worker $<TARGET_FILE:decid-moteur> "${CMAKE_CURRENT_SOURCE_DIR}"
         WORKING_DIRECTORY $<TARGET_FILE_DIR:decid-moteur>)
add_executable(test_worker tests/test_worker.cpp)
target_link_libraries(test_worker PRIVATE decid_noyau)
target_include_directories(test_worker PRIVATE tests)

if(NOT DECID_PORTABLE)   # en portable, OpenCV est lie statiquement : aucune DLL a copier
foreach(cible decid-moteur test_worker ${DECID_TESTS})
  add_custom_command(TARGET ${cible} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DECID_OPENCV_DLL}" $<TARGET_FILE_DIR:${cible}>)
endforeach()
endif()

# Application Slint (Decid.exe) : Slint 1.18.1 precompile dans externe/slint, ou ailleurs avec -DSlint_DIR=<...>/lib/cmake/Slint
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/externe/slint" OR Slint_DIR OR DECID_PORTABLE)
  add_subdirectory(app)
  # F5 dans Visual Studio lance Decid.exe (et non ALL_BUILD)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT decid-studio)
else()
  message(WARNING "Slint introuvable : seul le moteur DecidMoteur.exe est construit. Installer Slint 1.18.1 pour C++ "
                  "(Slint-cpp-1.18.1-win64-MSVC-AMD64.exe) dans externe/slint, ou passer -DSlint_DIR=<installation>/lib/cmake/Slint.")
endif()

# Rangement dans Visual Studio : Decid.exe et le moteur en haut, le reste dans des dossiers
# (ALL_BUILD, RUN_TESTS et ZERO_CHECK, crees par CMake, vont dans CMakePredefinedTargets)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
foreach(cible decid_base decid_noyau decid_outil decid2_app_services)
  if(TARGET ${cible})
    set_target_properties(${cible} PROPERTIES FOLDER "Bibliotheques")
  endif()
endforeach()
foreach(cible test_worker test_client_worker test_projet test_rendu_plan ${DECID_TESTS})
  if(TARGET ${cible})
    set_target_properties(${cible} PROPERTIES FOLDER "Tests")
  endif()
endforeach()
if(TARGET decid-empaqueter)
  set_target_properties(decid-empaqueter PROPERTIES FOLDER "Construction du portable")
endif()
