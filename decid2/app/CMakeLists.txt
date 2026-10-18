# Decid 2 - application Slint. Ajoutee par add_subdirectory(app) du projet racine, si Slint est trouve
# Decid.exe contient l'interface ET le moteur : pour chaque calcul, il se relance lui-meme en mode moteur
# (Decid.exe --moteur executer ...), dans un processus separe. La fenetre ne charge jamais OpenCV : la DLL est
# en chargement differe (build normal) et n'est appelee que par le processus moteur.
cmake_minimum_required(VERSION 3.21)

if(DECID_PORTABLE)   # Slint statique compilee depuis ses sources (build-portable/construire-slint.sh)
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../externe/slint-statique")
else()
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../externe/slint")
endif()
set(SLINT_STYLE fluent CACHE STRING "style Slint (aucun widget std-widgets n'est utilise)")
find_package(Slint REQUIRED)
if(DECID_PORTABLE)   # bibliotheques systeme Windows demandees par le runtime Rust de Slint statique (std, winit)
  set_property(TARGET slint_cpp-static APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    kernel32 ntdll userenv ws2_32 dbghelp imm32 uxtheme)
endif()

# --- services sans interface (testables) : projet, client du worker, contexte, rendu du plan, plateforme
add_library(decid2_app_services STATIC
  src/Projet.cpp
  src/ClientWorker.cpp
  src/Contexte.cpp
  src/RenduPlan.cpp
  src/Plateforme.cpp
)
target_include_directories(decid2_app_services PUBLIC src)
target_link_libraries(decid2_app_services PUBLIC decid_base ole32 shell32)
set_target_properties(decid2_app_services PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# --- tests sans interface (CTest), harnais ../tests/Verifier.h
foreach(t test_projet test_rendu_plan test_client_worker)
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE decid2_app_services)
  target_include_directories(${t} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
  set_target_properties(${t} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()
add_test(NAME app_projet COMMAND test_projet "${CMAKE_CURRENT_SOURCE_DIR}/.." WORKING_DIRECTORY $<TARGET_FILE_DIR:test_projet>)
add_test(NAME app_rendu_plan COMMAND test_rendu_plan "${CMAKE_CURRENT_SOURCE_DIR}/.." WORKING_DIRECTORY $<TARGET_FILE_DIR:test_rendu_plan>)
add_test(NAME app_client_worker COMMAND test_client_worker $<TARGET_FILE:decid-moteur> WORKING_DIRECTORY $<TARGET_FILE_DIR:test_client_worker>)

# --- DECID : Decid.exe
add_executable(decid-studio WIN32 studio/src/main.cpp studio/src/Moteur.cpp studio/src/Plan.cpp studio/src/Etude.cpp studio/src/Vue3D.cpp studio/src/Decouvrir.cpp studio/src/outils/BacASable.cpp studio/src/outils/Validation.cpp studio/src/outils/PasAPas.cpp studio/src/outils/Regression.cpp studio/src/outils/Generateur.cpp studio/src/outils/Decision.cpp studio/src/outils/AtelierImage.cpp studio/src/Webcam.cpp studio/src/Annexes.cpp studio/src/CadreFenetre.cpp studio/src/Graphique.cpp studio/src/Toile.cpp src/Capture.cpp studio/decid.manifest)
slint_target_sources(decid-studio studio/ui/decid.slint)
target_include_directories(decid-studio PRIVATE src studio/src)
target_link_libraries(decid-studio PRIVATE decid2_app_services decid_outil Slint::Slint opengl32 mfplat mf mfreadwrite mfuuid comctl32)
set_target_properties(decid-studio PROPERTIES OUTPUT_NAME Decid CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
  target_link_options(decid-studio PRIVATE /ENTRY:wmainCRTStartup)
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/slint_generated_decid_1.cpp PROPERTIES
    COMPILE_OPTIONS "/wd4458;/wd4244;/wd4100;/wd4189;/wd4267;/wd4456;/wd4457;/wd4702;/wd4996;/wd4127;/wd4060;/wd4305;/wd4065;/wd4101;/wd4146;/wd4018;/wd4245;/wd4389")
endif()
if(NOT DECID_PORTABLE)   # portable : Slint et OpenCV statiques
target_link_options(decid-studio PRIVATE /DELAYLOAD:opencv_world4100$<$<CONFIG:Debug>:d>.dll)
target_link_libraries(decid-studio PRIVATE delayimp)
add_custom_command(TARGET decid-studio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:slint_cpp-shared> $<TARGET_FILE_DIR:decid-studio>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DECID_OPENCV_DLL}" $<TARGET_FILE_DIR:decid-studio>)
endif()

# --- EXECUTABLE PORTABLE (option DECID_PORTABLE du projet racine, dossier build-portable) : les jeux databasesCSV
# sont compresses par decid-empaqueter puis embarques dans Decid.exe (ressources DECID_PAQUET, DECID_EMPREINTE) ;
# app/portable/Portable.cpp les extrait au lancement dans %LOCALAPPDATA%\Decid\portable\<empreinte>.
if(DECID_PORTABLE)
  set(DECID_BIBLIOTHEQUE "${CMAKE_CURRENT_SOURCE_DIR}/../../databasesCSV" CACHE PATH "bibliotheque databasesCSV embarquee dans Decid.exe")
  add_executable(decid-empaqueter portable/empaqueter.cpp)
  target_link_libraries(decid-empaqueter PRIVATE decid_base Cabinet)
  file(GLOB_RECURSE DECID_FICHIERS_BIBLIOTHEQUE CONFIGURE_DEPENDS "${DECID_BIBLIOTHEQUE}/*")
  set(DECID_PAQUET "${CMAKE_CURRENT_BINARY_DIR}/portable/decid.paquet")
  set(DECID_EMPREINTE "${CMAKE_CURRENT_BINARY_DIR}/portable/empreinte.txt")
  add_custom_command(OUTPUT "${DECID_PAQUET}" "${DECID_EMPREINTE}"
    COMMAND decid-empaqueter "${DECID_PAQUET}" "${DECID_EMPREINTE}" "${DECID_BIBLIOTHEQUE}"
    DEPENDS decid-empaqueter ${DECID_FICHIERS_BIBLIOTHEQUE}
    COMMENT "Paquet Decid : jeux d'exemple databasesCSV" VERBATIM)
  set(DECID_RC "${CMAKE_CURRENT_BINARY_DIR}/portable/decid-portable.rc")
  # icone et informations de version : un executable non signe sans editeur ni version est plus suspect pour
  # SmartScreen et les antivirus
  set(DECID_ICONE "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/brand/decid-logo-final/decid.ico" CACHE FILEPATH "icone de Decid.exe")
  string(REPLACE "." "," DECID_VERSION_VIRGULES "${PROJECT_VERSION}.0")
  file(WRITE "${DECID_RC}.tmp" "#include <winres.h>
1 ICON \"${DECID_ICONE}\"
DECID_PAQUET RCDATA \"${DECID_PAQUET}\"
DECID_EMPREINTE RCDATA \"${DECID_EMPREINTE}\"
VS_VERSION_INFO VERSIONINFO
FILEVERSION ${DECID_VERSION_VIRGULES}
PRODUCTVERSION ${DECID_VERSION_VIRGULES}
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
  BLOCK \"StringFileInfo\"
  BEGIN
    BLOCK \"040C04B0\"
    BEGIN
      VALUE \"CompanyName\", \"Decid\"
      VALUE \"FileDescription\", \"Decid - apprendre la classification\"
      VALUE \"FileVersion\", \"${PROJECT_VERSION}\"
      VALUE \"InternalName\", \"Decid\"
      VALUE \"OriginalFilename\", \"Decid.exe\"
      VALUE \"ProductName\", \"Decid\"
      VALUE \"ProductVersion\", \"${PROJECT_VERSION}\"
    END
  END
  BLOCK \"VarFileInfo\"
  BEGIN
    VALUE \"Translation\", 0x040C, 1200
  END
END
")
  configure_file("${DECID_RC}.tmp" "${DECID_RC}" COPYONLY)
  set_source_files_properties("${DECID_RC}" PROPERTIES OBJECT_DEPENDS "${DECID_PAQUET};${DECID_EMPREINTE};${DECID_ICONE}")
  set_source_files_properties("${DECID_PAQUET}" "${DECID_EMPREINTE}" PROPERTIES HEADER_FILE_ONLY ON)
  target_sources(decid-studio PRIVATE portable/Portable.cpp "${DECID_RC}" "${DECID_PAQUET}" "${DECID_EMPREINTE}")
  target_include_directories(decid-studio PRIVATE portable)
  target_compile_definitions(decid-studio PRIVATE DECID_PORTABLE)
  target_link_libraries(decid-studio PRIVATE Cabinet)
  # LNK4217 : les en-tetes Slint declarent dllimport des fonctions qui sont ici liees statiquement (sans effet)
  target_link_options(decid-studio PRIVATE /IGNORE:4217)
  # Media Foundation (webcam seulement) charge a la demande : Decid demarre aussi sur Windows « N »
  target_link_options(decid-studio PRIVATE /DELAYLOAD:mfplat.dll /DELAYLOAD:mf.dll /DELAYLOAD:mfreadwrite.dll)
  target_link_libraries(decid-studio PRIVATE delayimp)
endif()
