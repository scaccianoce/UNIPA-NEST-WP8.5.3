# DEAGUI — User Guide

## Table of Contents

1. [Starting the application](#1-starting-the-application)
2. [Navigation](#2-navigation)
3. [File Conversion](#3-file-conversion)
4. [Climate Visualisation](#4-climate-visualisation)
5. [GeoJSON Visualisation](#5-geojson-visualisation)
6. [Building Component Editor](#6-building-component-editor)
7. [Schedule Management](#7-schedule-management)
8. [Building Types Table](#8-building-types-table)
9. [Buildings Viewer](#9-buildings-viewer)
10. [Run Simulation](#10-run-simulation)
11. [Simulation Results & Resilience Indicators](#11-simulation-results--resilience-indicators)
12. [Settings](#12-settings)

---

## 1. Starting the application

Launch the `DEAGUI` executable. The main window opens at 1400 × 800 pixels and centres itself on the primary screen automatically. The window is resizable with a minimum size of 900 × 600 pixels.

---

## 2. Navigation

The left panel (dark-blue sidebar) lists all application sections. Click any item to switch to the corresponding page; the active page is highlighted in blue. The right area displays the page content with vertical and horizontal scrollbars as needed.

---

## 3. File Conversion

### EPW → WTST
1. Click **EPW → wtst**.
2. Select the source `.epw` file from the dialog.
3. Choose the destination folder and filename (`.wtst`).
4. `epw2wtst` runs in the background; its output appears in the console.

### IDF → JSON
1. Click **IDF → JSON**.
2. Select the `.idf` file.
3. Choose the output `.json` path.
4. (Optional) Enable EnergyPlus-assisted conversion from the Settings page.

### SHP → GeoJSON
1. Click **SHP → GeoJSON**.
2. Select the `.shp` Shapefile.
3. Specify the destination `.geojson` path.

### GeoJSON → GeoJSON (polygon processing)
Use the **GeoJSON → GeoJSON** button to reprocess existing GeoJSON files through `pol_geoelem`.

### Schedule:Compact → CSV
Converts `Schedule:Compact` blocks from an IDF file into an 8760-row CSV table.

---

## 4. Climate Visualisation

1. Click **📂 Load WTST file** and select a `.wtst` or `.csv` climate file.
2. The first 8 header lines are displayed in the top panel.
3. Three charts are generated automatically:
   - **Temperature** — outdoor air temperature (`temperature` column).
   - **Humidity** — relative humidity.
   - **Solar radiation** — select the two desired columns from the drop-down menus.
4. Use the **From hour** and **To hour** fields to restrict the X-axis range, then click **🔄 Refresh** or press Enter.

> **Tip**: charts are interactive — use the mouse wheel to zoom and drag to pan.

---

## 5. GeoJSON Visualisation

1. Click **📂 Load GeoJSON** and select a `.geojson` file.
2. The feature type and name are shown in the top boxes.
3. Polygons (Gauss-Boaga coordinates) are rendered in the graphics view.
4. Click a polygon to display its properties in the right panel.

---

## 6. Building Component Editor

This section manages the JSON file of building components (walls, windows, thermal bridges, HVAC, etc.).

1. Click **📂 Load** and select the components JSON file.
2. Components are organised into tabs by section (opaque, transparent, HVAC, …).
3. Edit cells directly in the table; some cells offer a drop-down with known values (e.g. `roughness`, `gas_type`).
4. Right-click to add or delete components and fields via the context menu.
5. Click **💾 Save** to overwrite the original file.

---

## 7. Schedule Management

1. Click **📂 Load Schedule CSV** and select a CSV file with 8760 hourly rows.
2. The content is shown in the text editor.
3. Edit values directly in the editor.
4. **💾 Save CSV** — saves the updated CSV file.
5. **💾 Save IDF** — exports `Schedule:Compact` blocks ready for insertion into an IDF file.

---

## 8. Building Types Table

1. Click **📂 Load CSV** and select the building typologies CSV file.
2. The table is shown with column headers.
3. Available operations:
   - **＋ Row** / **− Row** — add/remove the selected row.
   - **＋ Column** / **− Column** — add/remove the selected column.
4. Click **💾 Save** to write the changes back to disk.
5. The **Create** button is enabled automatically once all prerequisite files (climate, components, schedules, buildings) have been loaded in the Run Simulation page.

---

## 9. Buildings Viewer

1. Click **📂 Load Buildings JSON** and select the `.json` buildings file.
2. The drop-down shows all buildings found (`N buildings found`).
3. Select a building to view:
   - **Location information** (latitude, longitude, elevation, time zone).
   - **Surfaces table** (name, type, construction, adjacent zone).
   - **Thermal zones table** (name, area, volume, multiplier).
4. Use **＋** to add surfaces or zones, then **💾 Save** to update the file.

---

## 10. Run Simulation

1. Select the four input files using the **📂 Select file** buttons:
   - Climate file (`.wtst`)
   - Components file (`.json`)
   - Schedules file (`.csv`)
   - Buildings file (`.json`)
2. Choose the **simulation mode** (`iso` or `noise`).
3. Click **▶️ Run Simulation**.
4. `BldSimu` output appears in real time in the console.
5. On completion, output files are saved in the same folder as the `BldSimu` binary.

---

## 11. Simulation Results & Resilience Indicators

### 11.1 Loading data

| Button | File to load |
|---|---|
| 📂 Load main CSV | Multi-zone simulation output |
| 📂 Load reference CSV | (Optional) Reference building dataset for comparative indicators |
| 📂 Load occupancy CSV | (Optional) Occupancy schedule (`timestamp`, `occupied`) |

**Required main CSV columns:**

```
timestamp, T_out, x_ear, <zone>_T_op, <zone>_T_air, <zone>_x_iar,
<zone>_Phi_C, <zone>_Phi_H, <zone>_T_surf, <zone>_T_surf_dev
```

For multiple thermal zones, repeat the `<zone>_…` block with different names (e.g. `Zone1_T_op`, `Zone2_T_op`, etc.).

### 11.2 Calculation parameters

Review and adjust the parameters in the three panels before computing:

**Comfort thresholds**

| Parameter | Default | Description |
|---|---|---|
| Comfort temperature – IOD/SBOI | 26 °C | Threshold for Indoor Overheating Degree and SBOI |
| SET alert threshold | 28 °C | SET threshold for Recovery/Absorptivity time |
| Cold-safety threshold | 17.78 °C | Threshold for WUMTP/RCI (winter scenario) |
| Base temperature – AWD/SBOI | 26 °C | Base temperature for Ambient Warmness Degree |
| Conditioned floor area | 150 m² | Normalisation area for primary energy |
| Time step | 1 h | Duration of each data time step |

**HVAC systems & energy conversion**

| Parameter | Default | Description |
|---|---|---|
| Heating system | Gas boiler | Gas boiler or electric heat pump |
| Heating efficiency / COP_H | 0.85 | Boiler efficiency or heat pump COP |
| Cooling COP | 3.0 | Cooling system COP |
| Gas emission factor | 0.202 kg_CO₂/kWh | CO₂ emission factor for natural gas |
| Electricity emission factor | 0.233 kg_CO₂/kWh | CO₂ emission factor for the grid (IT default) |
| Primary energy factor – electricity | 2.36 | Primary energy conversion factor for electricity |
| Primary energy factor – gas | 1.0 | Primary energy conversion factor for natural gas |

**Occupant model (SET)**

| Parameter | Default | Description |
|---|---|---|
| Metabolic rate | 1.2 met | Occupant activity level |
| Clothing insulation | 0.5 clo | Clothing thermal resistance |
| Indoor air velocity | 0.1 m/s | Air speed inside the zone |

### 11.3 Computing indicators

Click **🧮 Compute Resilience Indicators**. The calculation runs in sequence:
1. `computeDerivedFields()` — computes T_dew, SET, NG_kWh, EC_kWh, CO₂_kg.
2. `computeAllIndicators()` — single-dataset indicators.
3. `computeTwoDatasetIndicators()` — comparative indicators (only if a reference file was loaded).

Results appear in the **Resilience Indicators** panel.

### 11.4 Interpreting results

| Indicator | Favourable value |
|---|---|
| IOD [°C·h] | Lower = less overheating |
| AWD [°C·h] | Climate-dependent; used as a reference |
| SBOI [%] | < 100 % → building mitigates outdoor heat |
| Primary energy [kWh/m²·yr] | Lower = more efficient |
| Recovery time [h] | Shorter = faster thermal recovery |
| Absorptivity time [h] | Shorter = less exposure to thermal stress |
| RCI [-] | > 1 → more resilient than the reference |
| TRI [-] | > 1 → retrofit improved resilience |
| OEF (aIOD) [-] | < 1 → resilient building; > 1 → vulnerable |
| ΔCO₂ [kg/yr] | Positive = emission reduction vs. reference |

### 11.5 Charts

Select the thermal zone and time range, then use the tabs:

| Tab | Variables displayed |
|---|---|
| 🌡️ Temperatures | T_out, T_op, T_air, T_surf, T_dew |
| 💧 Humidity | x_ear (outdoor), x_iar (indoor) |
| ⚡ Thermal Loads | Φ_C, Φ_H, EC_kWh, NG_kWh |
| 🌫️ CO₂ | CO₂ per zone, total building CO₂ |
| 🏠 Comfort (SET) | SET, T_op, T_out |

Click **🔄 Refresh charts** after changing the zone or time range.

---

## 12. Settings

### EnergyPlus
- Enable the **EnergyPlus** toggle to activate IDF conversion via EnergyPlus.
- Click **📂 Browse** to set the EnergyPlus installation folder.

### Calendar
- Select the **day of the week for 1 January** from the drop-down (used by the simulator to synchronise weekly schedules).
