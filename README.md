# DEAGUI — Urban District Energy Analysis

**DEAGUI** is a desktop application built with **Qt 6** and **C++20** to support energy analysis of buildings and urban districts. It integrates tools for converting meteorological and geographic files, managing input data for the **BldSimu** energy simulator, visualising simulation results, and computing the **thermal resilience indicators** defined in the NEST WP5 project. The present work has been developed within the project funded under the National Recovery and Resilience Plan (NRRP), Mission 4 Component 2 Investment 1.3 – Call for tender N. 341 of 15.03.2022 of Ministero dell'Università e della Ricerca (MUR); funded by the European Union – NextGenerationEU. Award Number: Project code PE0000021, Concession Decree No. 1561 of 11.10.2022 adopted by Ministero dell’Università e della Ricerca (MUR), CUP E63C22002160007, Project title "Network 4 Energy Sustainable Transition – NEST".

---

## Key Features

| Section | Description |
|---|---|
| 🔄 File Conversion | EPW → WTST, IDF → JSON, SHP → GeoJSON, Schedule:Compact → CSV |
| 📊 Climate Visualisation | Interactive charts of temperature, humidity and solar radiation from WTST files |
| 🗺️ GeoJSON Visualisation | Map rendering of polygons with element properties |
| 🏗️ Component Editor | JSON management of building components (walls, windows, HVAC, etc.) |
| 📅 Schedule Management | 8760-hour schedule editor with IDF export |
| 📋 Building Types Table | CSV table of building typologies with dynamic row/column management |
| 🏢 Buildings Viewer | JSON browser of simulated buildings with surfaces and thermal zones |
| ▶️ Run Simulation | Launch BldSimu with input file selection and simulation mode |
| 📈 Simulation Results | Time-series visualisation and computation of NEST WP5 resilience indicators |
| ⚙️ Settings | EnergyPlus path configuration and calendar settings |

---

## System Requirements

- **Operating system**: Windows 10/11, macOS 12+, Linux (Ubuntu 22.04+)
- **Qt**: version 6.5 or later (modules: Core, Gui, Widgets, Charts)
- **Compiler**: C++20 — GCC 12+, Clang 14+, MSVC 2022+
- **CMake**: version 3.14+

---

## Build Instructions

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

On **Windows**, `windeployqt` is invoked automatically after the build to collect the required Qt DLLs.

---

## Repository Structure

```
GUI_UDES/
├── main.cpp                  # Qt entry point
├── mainwindow.h              # Main window declaration
├── mainwindow.cpp            # GUI implementation (13 pages)
├── resilience_indicators.h   # Header-only library: thermal resilience indicators
├── CMakeLists.txt            # CMake configuration
├── bin/                      # External tool binaries (BldSimu, epw2wtst, …)
├── icons/                    # Application icons
└── cmake-build-debug/        # Build directory (generated)
```

---

## External Tools (`bin/` folder)

| Binary | Function |
|---|---|
| `BldSimu` | Urban district energy simulator |
| `epw2wtst` | EnergyPlus EPW climate file → WTST converter |
| `idf2json` | IDF input file → JSON converter |
| `shaperead` | Shapefile → GeoJSON reader/converter |
| `pol_geoelem` | Geographic polygon processing |

---

## Implemented Resilience Indicators

Indicators are defined in the **NEST WP5** project; equations are taken from:

> *Peri et al., Supplementary Material, Energy and Buildings (2025)*

| Code | Name | Category |
|---|---|---|
| In_I-B-1_07 | Unmet hours for Humidex | Indoor comfort |
| In_I-B-2_03 | Indoor Overheating Degree (IOD) | Indoor comfort |
| In_I-B-2_11 | Recovery time | Indoor comfort |
| In_I-B-2_13 | Absorptivity time | Indoor comfort |
| In_I-B-2_08 | Overheating Escalation Factor (aIOD) | Indoor comfort |
| In_O-B-1_01 | Ambient Warmness Degree (AWD) | Outdoor UHI |
| In_O-B-2_02 | Reduction in annual CO₂ emissions | Outdoor energy |
| In_O-B-2_03 | Primary energy consumption | Outdoor energy |
| In_O-B-3_07 | Reduction in external surface temperature | Outdoor UHI |
| In_I-A-1_01 | Seasonal Building Overheating Index (SBOI) | Indoor comfort |
| In_I-A-2_01 | Resilience Class Index (RCI) | Indoor comfort |
| In_I-A-2_02 | Thermal Resilience Index (TRI) | Indoor comfort |

---

## Licence and References

Developed within the **NEST WP5** project — University of Palermo (UNIPA).

Main bibliographic reference:
> Peri G. et al., *Thermal resilience indicators for building energy analysis*, Supplementary Material, Energy and Buildings, 2025.
