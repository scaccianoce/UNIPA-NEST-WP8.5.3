/**
 * @file resilience_indicators.h
 * @brief Thermal resilience indicators for building energy analysis
 *
 * Indicators from the first table of "NEST WP5 - indicatori.docx",
 * with equations from: Peri et al., Supplementary Material,
 * Energy and Buildings (2025).
 *
 * Indicators implemented:
 *  - In_I-B-1_07 : Unmet hours for Humidex
 *  - In_I-B-2_03 : Indoor Overheating Degree (IOD)
 *  - In_I-B-2_11 : Recovery time
 *  - In_I-B-2_13 : Absorptivity time
 *  - In_I-B-2_08 : Overheating Escalation Factor (aIOD)
 *  - In_O-B-1_01 : Ambient Warmness Degree (AWD)
 *  - In_O-B-2_02 : Reduction in annual CO2 emissions
 *  - In_O-B-2_03 : Primary energy consumption (heating/cooling)
 *  - In_O-B-3_07 : Reduction in external surface temperature of envelope
 *  - In_I-A-1_01 : Seasonal Building Overheating Index (SBOI)
 *  - In_I-A-2_01 : Resilience Class Index (RCI)
 *  - In_I-A-2_02 : Thermal Resilience Index (TRI)
 */

#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// ============================================================
//  Data structures
// ============================================================

/**
 * @brief Per-zone data for one hourly time step.
 *
 * Each thermal zone in the building has one ZoneData entry per time step.
 * CSV fields are read directly from the input file; derived fields are
 * filled by computeDerivedFields().
 */
struct ZoneData {
    // ── CSV inputs ────────────────────────────────────────────────────────────
    double T_op       = 0.0; ///< Operative indoor temperature [°C]
    double T_air      = 0.0; ///< Indoor dry-bulb air temperature [°C]
    double x_iar      = 0.0; ///< Indoor air humidity ratio [kg_v/kg_as]
    double Phi_C      = 0.0; ///< Cooling thermal load compensated by the HVAC system [kWh/h]
    double Phi_H      = 0.0; ///< Heating thermal load compensated by the HVAC system [kWh/h]
    double T_surf     = 0.0; ///< Mean external surface temperature of the zone envelope [°C]
    double T_surf_dev = 0.0; ///< Std dev of external surface temperatures of the zone envelope [°C]

    // ── Derived fields (filled by computeDerivedFields) ───────────────────────
    double SET    = 0.0; ///< Standard Effective Temperature [°C]
    double NG_kWh = 0.0; ///< Natural gas end-use energy [kWh/h]
    double EC_kWh = 0.0; ///< Electricity end-use energy [kWh/h]
    double CO2_kg = 0.0; ///< CO₂ emissions [kg/h]
};

/**
 * @brief One row of hourly simulation output (all thermal zones).
 *
 * Fields are split into three groups:
 *  1. **Global CSV inputs** – outdoor conditions, read directly from the data file.
 *  2. **Per-zone data** – vector of ZoneData, one entry per thermal zone.
 *  3. **Aggregated derived fields** – computed by computeDerivedFields();
 *     temperature fields are zone averages, energy/CO₂ fields are zone sums.
 *     These aggregated fields are used directly by all indicator functions.
 *  4. **External input** – occupancy flag, injected via applyOccupancy().
 *
 * All temperatures in [°C] unless explicitly noted.
 * Energy values refer to a single time-step (1 h by default).
 */
struct HourlyData {
    // ── Global CSV inputs ─────────────────────────────────────────────────────
    double T_out = 0.0; ///< Outdoor dry-bulb air temperature [°C]
    double x_ear = 0.0; ///< Outdoor air humidity ratio [kg_v/kg_as]

    // ── Per-zone data ─────────────────────────────────────────────────────────
    std::vector<ZoneData> zones; ///< One entry per thermal zone

    // ── External input (set via applyOccupancy) ───────────────────────────────
    bool occupied = false; ///< Occupancy flag (true = occupied)

    // ── Aggregated derived fields (filled by computeDerivedFields) ────────────
    // Temperature/humidity: mean across zones; energy/CO₂: sum across zones.
    double T_dew  = 0.0; ///< Outdoor dew-point temperature [°C]
    double T_op   = 0.0; ///< Mean operative temperature across zones [°C]
    double T_air  = 0.0; ///< Mean indoor air temperature across zones [°C]
    double T_surf = 0.0; ///< Mean external surface temperature across zones [°C]
    double x_iar  = 0.0; ///< Mean indoor humidity ratio across zones [kg_v/kg_as]
    double Phi_C  = 0.0; ///< Total cooling load across zones [kWh/h]
    double Phi_H  = 0.0; ///< Total heating load across zones [kWh/h]
    double SET    = 0.0; ///< Mean Standard Effective Temperature across zones [°C]
    double NG_kWh = 0.0; ///< Total natural gas end-use energy across zones [kWh/h]
    double EC_kWh = 0.0; ///< Total electricity end-use energy across zones [kWh/h]
    double CO2_kg = 0.0; ///< Total CO₂ emissions across zones [kg/h]
};

/**
 * @brief HVAC system type for the heating plant.
 */
enum class HeatingSystem {
    GAS_BOILER,  ///< Natural gas boiler (efficiency = eta_H)
    ELECTRIC_HP  ///< Electric heat pump  (COP = eta_H)
};

/**
 * @brief Parameters shared across multiple indicator computations.
 */
struct ResilienceParams {
    // ── Thermal comfort thresholds ────────────────────────────────────────────
    double T_comfort    = 26.0;   ///< Indoor comfort threshold for IOD / SBOI [°C]
    double T_SET_alert  = 28.0;   ///< SET alert threshold for Recovery / Absorptivity time [°C]
    double T_cold_safe  = 17.78;  ///< Cold-safety threshold (vulnerable population) [°C]
    double T_base       = 26.0;   ///< Base temperature for AWD  (often = T_comfort) [°C]
    double floor_area   = 1.0;    ///< Conditioned floor area [m²]
    double elec_conv    = 2.36;   ///< Primary-energy conversion factor for electricity [-]
    double gas_conv     = 1.0;    ///< Primary-energy conversion factor for natural gas [-]
    double dt_h         = 1.0;    ///< Time step duration [h]  (default: hourly data)

    // ── HVAC system parameters ────────────────────────────────────────────────
    HeatingSystem heating_system = HeatingSystem::GAS_BOILER;
                                  ///< Type of heating plant installed
    double eta_H  = 0.85;         ///< Heating efficiency [-] (boiler η) or COP [-] (heat pump)
    double COP_C  = 3.0;          ///< Cooling system COP [-]

    // ── CO₂ emission factors ──────────────────────────────────────────────────
    double ef_gas  = 0.202;  ///< CO₂ emission factor – natural gas [kg_CO₂/kWh]
    double ef_elec = 0.233;  ///< CO₂ emission factor – electricity [kg_CO₂/kWh]

    // ── Parameters for SET (Standard Effective Temperature) computation ───────
    double met   = 1.2;       ///< Occupant metabolic rate [met]  (1 met = 58.15 W/m²)
    double clo   = 0.5;       ///< Clothing insulation [clo]      (1 clo = 0.155 m²K/W)
    double v_air = 0.1;       ///< Indoor air velocity [m/s]
    double P_atm = 101325.0;  ///< Atmospheric pressure [Pa]
};

// ============================================================
//  CSV loader (multi-zone, header-driven)
// ============================================================

/**
 * @brief Split a CSV line by commas, returning string_views into the original.
 *
 * Handles Windows CRLF by stripping a trailing '\\r'.
 */
inline std::vector<std::string_view> splitCSVLine(const std::string& line)
{
    std::vector<std::string_view> parts;
    const char* p   = line.data();
    const char* end = p + line.size();
    if (end > p && *(end - 1) == '\r') --end;
    while (p <= end) {
        const char* q = p;
        while (q < end && *q != ',') ++q;
        parts.emplace_back(p, static_cast<std::size_t>(q - p));
        p = q + 1;
    }
    return parts;
}

/**
 * @brief Parse a string_view to double using std::from_chars (fast, locale-independent).
 *
 * Returns 0.0 on parse failure or empty input.
 */
inline double svToDouble(std::string_view sv)
{
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) sv.remove_prefix(1);
    while (!sv.empty() && (sv.back()  == ' ' || sv.back()  == '\t')) sv.remove_suffix(1);
    if (sv.empty()) return 0.0;
    double val = 0.0;
    std::from_chars(sv.data(), sv.data() + sv.size(), val);
    return val;
}

/**
 * @brief Load hourly simulation data from a multi-zone CSV file.
 *
 * Expected CSV format (comma-separated, with header row):
 * @code
 * timestamp,T_out,x_ear,<zone>_T_op,<zone>_T_air,<zone>_x_iar,<zone>_Phi_C,<zone>_Phi_H,<zone>_T_surf,<zone>_T_surf_dev,...
 * 2024-01-01 00:00,5.2,0.004,Zone1_T_op,...
 * @endcode
 *
 * Column descriptions:
 *  - timestamp      : hour index or datetime string (ignored numerically)
 *  - T_out          : outdoor dry-bulb air temperature [°C]
 *  - x_ear          : outdoor specific humidity [kg_v/kg_as]
 *  Per thermal zone (prefix = zone name, e.g. "Zone1"):
 *  - {z}_T_op       : operative indoor temperature [°C]
 *  - {z}_T_air      : indoor dry-bulb air temperature [°C]
 *  - {z}_x_iar      : indoor specific humidity [kg_v/kg_as]
 *  - {z}_Phi_C      : cooling thermal load compensated by HVAC [kWh/h]
 *  - {z}_Phi_H      : heating thermal load compensated by HVAC [kWh/h]
 *  - {z}_T_surf     : mean external surface temperature of zone envelope [°C]
 *  - {z}_T_surf_dev : std dev of external surface temperatures [°C]
 *
 * Zone names are extracted automatically from the header by stripping the
 * field suffixes (_T_op, _T_air, _x_iar, _Phi_C, _Phi_H, _T_surf, _T_surf_dev).
 * Unrecognised columns are silently ignored.
 *
 * The function also accepts the legacy single-zone format
 * (bare column names T_op, T_air, x_iar, Phi_C, Phi_H, T_surf without a zone
 * prefix) for backward compatibility.
 *
 * After loading, call applyOccupancy() to inject the occupancy schedule,
 * then computeDerivedFields() to obtain aggregated T_dew, SET, energy and CO₂.
 *
 * @param filename  Path to the CSV file.
 * @return          Vector of HourlyData records (one per data row).
 *                  Each record contains one ZoneData entry per detected zone.
 * @throws std::runtime_error on file-open or parse failure.
 */
inline std::vector<HourlyData> loadCSV(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filename);

    std::string line;
    if (!std::getline(file, line))
        throw std::runtime_error("CSV file is empty: " + filename);

    // ── Parse header ──────────────────────────────────────────────────────────
    const auto hdr_tokens = splitCSVLine(line);
    const std::size_t ncols = hdr_tokens.size();

    // Global column indices
    int idx_T_out = -1, idx_x_ear = -1;

    // Per-zone column index record (all indices default to -1 = absent)
    struct ZoneColIdx {
        int T_op = -1, T_air = -1, x_iar = -1,
            Phi_C = -1, Phi_H = -1, T_surf = -1, T_surf_dev = -1;
    };

    // Zone field suffixes checked longest-first to avoid ambiguous matches.
    // Value = field slot identifier (0-6).
    static const std::pair<std::string_view, int> kSuffixes[] = {
        {"_T_surf_dev", 6},
        {"_T_surf",     5},
        {"_T_op",       0},
        {"_T_air",      1},
        {"_x_iar",      2},
        {"_Phi_C",      3},
        {"_Phi_H",      4},
    };

    std::unordered_map<std::string, ZoneColIdx> zone_map;
    std::vector<std::string> zone_order; // preserves column order

    for (std::size_t c = 0; c < ncols; ++c) {
        const std::string col(hdr_tokens[c]);

        if (col == "timestamp") continue;
        if (col == "T_out") { idx_T_out = static_cast<int>(c); continue; }
        if (col == "x_ear") { idx_x_ear = static_cast<int>(c); continue; }

        // Try to match a zone field suffix
        bool matched = false;
        for (const auto& [sfx, fid] : kSuffixes) {
            if (col.size() > sfx.size() &&
                std::string_view(col).substr(col.size() - sfx.size()) == sfx)
            {
                const std::string zname = col.substr(0, col.size() - sfx.size());
                if (zone_map.find(zname) == zone_map.end()) {
                    zone_map[zname] = ZoneColIdx{};
                    zone_order.push_back(zname);
                }
                ZoneColIdx& zi = zone_map[zname];
                switch (fid) {
                    case 0: zi.T_op       = static_cast<int>(c); break;
                    case 1: zi.T_air      = static_cast<int>(c); break;
                    case 2: zi.x_iar      = static_cast<int>(c); break;
                    case 3: zi.Phi_C      = static_cast<int>(c); break;
                    case 4: zi.Phi_H      = static_cast<int>(c); break;
                    case 5: zi.T_surf     = static_cast<int>(c); break;
                    case 6: zi.T_surf_dev = static_cast<int>(c); break;
                }
                matched = true;
                break;
            }
        }

        // Legacy single-zone: bare field names without zone prefix
        if (!matched) {
            static const std::pair<std::string_view, int> kBare[] = {
                {"T_op",  0}, {"T_air", 1}, {"x_iar", 2},
                {"Phi_C", 3}, {"Phi_H", 4}, {"T_surf", 5},
            };
            for (const auto& [name, fid] : kBare) {
                if (col == name) {
                    if (zone_map.find("_zone") == zone_map.end()) {
                        zone_map["_zone"] = ZoneColIdx{};
                        zone_order.push_back("_zone");
                    }
                    ZoneColIdx& zi = zone_map["_zone"];
                    switch (fid) {
                        case 0: zi.T_op   = static_cast<int>(c); break;
                        case 1: zi.T_air  = static_cast<int>(c); break;
                        case 2: zi.x_iar  = static_cast<int>(c); break;
                        case 3: zi.Phi_C  = static_cast<int>(c); break;
                        case 4: zi.Phi_H  = static_cast<int>(c); break;
                        case 5: zi.T_surf = static_cast<int>(c); break;
                    }
                    break;
                }
            }
        }
    }

    if (idx_T_out < 0) throw std::runtime_error("Header missing 'T_out' column.");
    if (idx_x_ear < 0) throw std::runtime_error("Header missing 'x_ear' column.");
    if (zone_order.empty()) throw std::runtime_error("No thermal zone columns found in header.");

    // Build ordered zone index array
    const std::size_t nzones = zone_order.size();
    std::vector<ZoneColIdx> zone_idx;
    zone_idx.reserve(nzones);
    for (const auto& zn : zone_order)
        zone_idx.push_back(zone_map.at(zn));

    // ── Parse data rows ───────────────────────────────────────────────────────
    std::vector<HourlyData> data;
    data.reserve(8760);

    std::size_t row = 1;
    while (std::getline(file, line)) {
        ++row;
        if (line.empty() || line.front() == '#') continue;

        const auto tok = splitCSVLine(line);
        if (tok.size() < ncols)
            throw std::runtime_error("Row " + std::to_string(row) +
                                     " has fewer columns than header (" +
                                     std::to_string(tok.size()) + " < " +
                                     std::to_string(ncols) + ").");

        HourlyData d;
        d.T_out = svToDouble(tok[static_cast<std::size_t>(idx_T_out)]);
        d.x_ear = svToDouble(tok[static_cast<std::size_t>(idx_x_ear)]);

        d.zones.resize(nzones);
        for (std::size_t z = 0; z < nzones; ++z) {
            const ZoneColIdx& zi = zone_idx[z];
            ZoneData& zd = d.zones[z];
            auto get = [&](int idx) -> double {
                return (idx >= 0) ? svToDouble(tok[static_cast<std::size_t>(idx)]) : 0.0;
            };
            zd.T_op       = get(zi.T_op);
            zd.T_air      = get(zi.T_air);
            zd.x_iar      = get(zi.x_iar);
            zd.Phi_C      = get(zi.Phi_C);
            zd.Phi_H      = get(zi.Phi_H);
            zd.T_surf     = get(zi.T_surf);
            zd.T_surf_dev = get(zi.T_surf_dev);
        }

        data.push_back(std::move(d));
    }

    return data;
}

/**
 * @brief Return the number of thermal zones detected in the loaded dataset.
 *
 * @param data  Dataset returned by loadCSV().
 * @return      Number of zones (0 if data is empty).
 */
inline std::size_t numZones(const std::vector<HourlyData>& data)
{
    return data.empty() ? 0 : data.front().zones.size();
}

// ============================================================
//  Occupancy injection
// ============================================================

/**
 * @brief Inject an externally loaded occupancy schedule into a dataset.
 *
 * The occupancy vector must have the same length as @p data.  Each element
 * maps directly to the corresponding HourlyData row.
 *
 * @param data       Dataset to annotate.
 * @param occupancy  Occupancy flags (true = occupied), one per hour.
 * @throws std::invalid_argument if sizes differ.
 */
inline void applyOccupancy(std::vector<HourlyData>& data,
                           const std::vector<bool>& occupancy)
{
    if (data.size() != occupancy.size())
        throw std::invalid_argument(
            "applyOccupancy: occupancy vector length (" +
            std::to_string(occupancy.size()) +
            ") does not match dataset length (" +
            std::to_string(data.size()) + ").");

    for (std::size_t i = 0; i < data.size(); ++i)
        data[i].occupied = occupancy[i];
}

/**
 * @brief Load an occupancy schedule from a two-column CSV file.
 *
 * Expected format (with header):
 * @code
 * timestamp,occupied
 * 0,0
 * 1,1
 * ...
 * @endcode
 *
 * @param filename  Path to the occupancy CSV file.
 * @return          Vector of bool values (true = occupied), one per row.
 * @throws std::runtime_error on file-open or parse failure.
 */
inline std::vector<bool> loadOccupancy(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open occupancy file: " + filename);

    std::vector<bool> occ;
    std::string line;

    if (!std::getline(file, line))  // skip header
        throw std::runtime_error("Occupancy file is empty: " + filename);

    std::size_t row = 1;
    while (std::getline(file, line)) {
        ++row;
        if (line.empty() || line.front() == '#') continue;
        const auto comma = line.find(',');
        if (comma == std::string::npos)
            throw std::runtime_error("Occupancy row " + std::to_string(row) +
                                     ": expected 2 columns.");
        occ.push_back(std::stoi(line.substr(comma + 1)) != 0);
    }
    return occ;
}

// ============================================================
//  Thermodynamic helpers
// ============================================================

/**
 * @brief Compute the saturation vapor pressure via the Magnus formula.
 *
 * @param T  Temperature [°C]
 * @return   Saturation vapor pressure [Pa]
 */
inline double satVaporPressure(double T)
{
    return 610.94 * std::exp(17.625 * T / (T + 243.04));
}

/**
 * @brief Convert outdoor humidity ratio to dew-point temperature.
 *
 * Uses the Magnus inverse formula (Alduchov & Eskridge 1996).
 *
 * @param x_ear  Outdoor humidity ratio [kg_v/kg_as]
 * @param P_atm  Atmospheric pressure [Pa]  (default 101325 Pa)
 * @return       Dew-point temperature [°C]
 */
inline double dewPointFromHumRatio(double x_ear, double P_atm = 101325.0)
{
    const double Pw    = x_ear * P_atm / (0.622 + x_ear);  // partial pressure [Pa]
    const double gamma = std::log(Pw / 610.94);
    return 243.04 * gamma / (17.625 - gamma);
}

// ============================================================
//  Standard Effective Temperature (SET) – Gagge 2-node model
// ============================================================

/**
 * @brief Compute the Standard Effective Temperature (SET).
 *
 * Implements the steady-state Gagge 2-node model
 * (Gagge, Fobelets & Berglund 1986; ASHRAE 55-2020 Appendix D).
 *
 * Algorithm outline:
 *  1. Derive skin temperature Tsk from the core-node energy balance at
 *     regulated core temperature (37 °C) with minimal skin conductance
 *     K_cr = 5.28 W/(m²K).
 *  2. Iterate clothing surface temperature Tcl.
 *  3. Compute required skin wettedness w.
 *  4. Find SET by bisection: temperature of a uniform reference environment
 *     (ta = tr, v = 0.1 m/s, φ = 50 %) that yields the same Tsk.
 *
 * @param ta     Indoor air temperature [°C]
 * @param tr     Mean radiant temperature [°C]
 * @param vel    Air velocity [m/s]
 * @param x_iar  Indoor humidity ratio [kg_v/kg_as]
 * @param met    Metabolic rate [met]  (1 met = 58.15 W/m²)
 * @param clo    Clothing insulation [clo]  (1 clo = 0.155 m²K/W)
 * @param P_atm  Atmospheric pressure [Pa]
 * @return       SET [°C]
 */
inline double computeSET(double ta, double tr, double vel, double x_iar,
                         double met = 1.2, double clo = 0.5,
                         double P_atm = 101325.0)
{
    constexpr double LR    = 16.5;      // Lewis ratio [K/kPa]
    constexpr double SIGMA = 5.67e-8;   // Stefan-Boltzmann constant [W/(m²K⁴)]
    constexpr double eps   = 0.95;      // emissivity of skin / clothing [-]
    constexpr double Tcr   = 37.0;      // regulated core temperature [°C]
    constexpr double K_cr  = 5.28;      // skin-to-core conductance at min blood flow [W/(m²K)]

    // Saturation vapor pressure [kPa] (Magnus, T in °C)
    auto Psat_kPa = [](double T) -> double {
        return 0.1333 * std::exp(18.956 - 4030.18 / (T + 235.0));
    };

    const double Pa_kPa = (x_iar * P_atm / (0.622 + x_iar)) * 1e-3;  // indoor vapor pressure [kPa]
    const double M  = met * 58.15;   // metabolic rate [W/m²]
    const double MW = M;             // external work assumed zero
    const double Icl = 0.155 * clo;
    const double fcl = (clo > 0) ? 1.05 + 0.1 * clo : 1.0;

    // Solve for skin temperature and wettedness in a given environment
    auto solveEnv = [&](double ta_, double tr_, double vel_, double Pa_)
        -> std::pair<double, double>   // {Tsk, w}
    {
        const double Cres = 0.0014 * M * (34.0 - ta_);
        const double Eres = 0.0173 * M * (5.87  - Pa_);
        const double Q    = MW - Cres - Eres;   // heat flow from core to skin [W/m²]
        const double Tsk  = Tcr - Q / K_cr;     // skin temperature from core balance

        // Iterate clothing surface temperature
        const double vel_eff = std::max(vel_, 0.1);
        const double hc_f    = 8.600001 * std::pow(vel_eff, 0.53);   // forced conv [W/(m²K)]
        double Tcl = Tsk;
        for (int i = 0; i < 150; ++i) {
            const double hc_n  = 3.0 * std::pow(std::max(std::abs(Tcl - ta_), 0.0), 0.25);
            const double hc    = std::max(hc_n, hc_f);
            const double T_mk  = (Tcl + tr_) / 2.0 + 273.15;
            const double hr    = 4.0 * eps * SIGMA * T_mk * T_mk * T_mk;
            const double Tcl_n = (Tsk + Icl * fcl * (hc * ta_ + hr * tr_))
                                 / (1.0 + Icl * fcl * (hc + hr));
            if (std::abs(Tcl_n - Tcl) < 1e-4) { Tcl = Tcl_n; break; }
            Tcl = Tcl_n;
        }

        // Heat transfer coefficients at converged Tcl
        const double hc_n = 3.0 * std::pow(std::max(std::abs(Tcl - ta_), 0.0), 0.25);
        const double hc   = std::max(hc_n, hc_f);
        const double T_mk = (Tcl + tr_) / 2.0 + 273.15;
        const double hr   = 4.0 * eps * SIGMA * T_mk * T_mk * T_mk;

        const double C_sk = fcl * hc * (Tcl - ta_);
        const double R_sk = fcl * hr * (Tcl - tr_);
        const double Ereq = Q - C_sk - R_sk;
        const double Emax = fcl * LR * hc * (Psat_kPa(Tsk) - Pa_);
        const double w    = std::max(0.06, std::min(1.0,
                                (Emax > 1e-10) ? Ereq / Emax : 1.0));

        return {Tsk, w};
    };

    const auto [Tsk_act, w_act] = solveEnv(ta, tr, vel, Pa_kPa);

    // Bisection: find SET such that solveEnv(SET, SET, 0.1, 0.5*Psat(SET)).Tsk == Tsk_act
    double lo = -20.0, hi = 80.0;
    for (int k = 0; k < 60; ++k) {
        const double mid    = 0.5 * (lo + hi);
        const double Pa_ref = 0.5 * Psat_kPa(mid);   // φ = 50 %
        const auto [Tsk_ref, w_ref] = solveEnv(mid, mid, 0.1, Pa_ref);
        (Tsk_ref < Tsk_act) ? lo = mid : hi = mid;
        if (hi - lo < 0.005) break;
    }
    return 0.5 * (lo + hi);
}

// ============================================================
//  Derived-fields computation
// ============================================================

/**
 * @brief Compute all derived fields for every row in @p data.
 *
 * This function must be called after loadCSV() and applyOccupancy().
 *
 * Per-zone computations (stored in each ZoneData entry):
 *  - **SET**    – Standard Effective Temperature via Gagge 2-node model.
 *  - **NG_kWh** – Natural gas end-use energy from Phi_H and heating efficiency.
 *  - **EC_kWh** – Electricity end-use energy from Phi_C (and Phi_H for HP).
 *  - **CO2_kg** – CO₂ emissions from NG_kWh and EC_kWh.
 *
 * Aggregated fields (stored in HourlyData, used by indicator functions):
 *  - **T_dew**  – Dew-point temperature from outdoor humidity ratio (Magnus).
 *  - Temperature/humidity fields (T_op, T_air, T_surf, x_iar, SET):
 *    simple mean across zones.
 *  - Energy/CO₂ fields (Phi_C, Phi_H, NG_kWh, EC_kWh, CO2_kg):
 *    sum across zones (whole-building totals).
 *
 * Mean radiant temperature estimated per zone as:
 * @code
 *   T_mrt = 2 * T_op - T_air     (from operative temperature definition)
 * @endcode
 *
 * Energy conversion (per zone):
 * @code
 *   // Gas boiler
 *   NG_kWh = Phi_H / eta_H
 *   EC_kWh = Phi_C / COP_C
 *
 *   // Electric heat pump
 *   NG_kWh = 0
 *   EC_kWh = Phi_H / COP_H + Phi_C / COP_C
 *
 *   CO2_kg = NG_kWh * ef_gas + EC_kWh * ef_elec
 * @endcode
 *
 * @param data    Dataset to update (modified in place).
 * @param params  System and comfort parameters.
 */
inline void computeDerivedFields(std::vector<HourlyData>& data,
                                 const ResilienceParams& params)
{
    for (auto& d : data) {
        // Global: dew-point temperature from outdoor humidity ratio
        d.T_dew = dewPointFromHumRatio(d.x_ear, params.P_atm);

        const std::size_t nz = d.zones.size();
        if (nz == 0) continue;

        double sum_T_op = 0, sum_T_air = 0, sum_T_surf = 0, sum_x_iar = 0, sum_SET = 0;
        double sum_Phi_C = 0, sum_Phi_H = 0, sum_NG = 0, sum_EC = 0, sum_CO2 = 0;

        for (auto& zd : d.zones) {
            // Mean radiant temperature from operative and air temperatures
            const double T_mrt = 2.0 * zd.T_op - zd.T_air;

            zd.SET = computeSET(zd.T_air, T_mrt, params.v_air, zd.x_iar,
                                params.met, params.clo, params.P_atm);

            if (params.heating_system == HeatingSystem::GAS_BOILER) {
                zd.NG_kWh = (params.eta_H > 0) ? zd.Phi_H / params.eta_H : 0.0;
                zd.EC_kWh = (params.COP_C  > 0) ? zd.Phi_C / params.COP_C  : 0.0;
            } else {  // ELECTRIC_HP
                zd.NG_kWh = 0.0;
                zd.EC_kWh = ((params.eta_H > 0) ? zd.Phi_H / params.eta_H : 0.0)
                           + ((params.COP_C  > 0) ? zd.Phi_C / params.COP_C  : 0.0);
            }
            zd.CO2_kg = zd.NG_kWh * params.ef_gas + zd.EC_kWh * params.ef_elec;

            sum_T_op   += zd.T_op;
            sum_T_air  += zd.T_air;
            sum_T_surf += zd.T_surf;
            sum_x_iar  += zd.x_iar;
            sum_SET    += zd.SET;
            sum_Phi_C  += zd.Phi_C;
            sum_Phi_H  += zd.Phi_H;
            sum_NG     += zd.NG_kWh;
            sum_EC     += zd.EC_kWh;
            sum_CO2    += zd.CO2_kg;
        }

        const double inv_nz = 1.0 / static_cast<double>(nz);
        // Temperatures and humidity: zone averages
        d.T_op   = sum_T_op   * inv_nz;
        d.T_air  = sum_T_air  * inv_nz;
        d.T_surf = sum_T_surf * inv_nz;
        d.x_iar  = sum_x_iar  * inv_nz;
        d.SET    = sum_SET    * inv_nz;
        // Energy and CO₂: whole-building sums
        d.Phi_C  = sum_Phi_C;
        d.Phi_H  = sum_Phi_H;
        d.NG_kWh = sum_NG;
        d.EC_kWh = sum_EC;
        d.CO2_kg = sum_CO2;
    }
}



/**
 * @brief Compute the Humidex value for a single hour.
 *
 * Formula (Borghero et al. [63] / Canadian meteorological service):
 * @code
 *   E = 6.105 * exp( 25.22*(Tdew_K - 273.16)/Tdew_K - 5.31*ln(Tdew_K/273.16) )
 *   H = T_air + 0.5555 * (E - 10.0)
 * @endcode
 *
 * @param T_air_C    Indoor dry-bulb air temperature [°C]
 * @param T_dew_C    Dew-point temperature [°C]
 * @return           Humidex value [-]
 */
inline double computeHumidex(double T_air_C, double T_dew_C)
{
    const double Tdew_K = T_dew_C + 273.16;
    const double E = 6.105 * std::exp(25.22 * (Tdew_K - 273.16) / Tdew_K
                                      - 5.31 * std::log(Tdew_K / 273.16));
    return T_air_C + 0.5555 * (E - 10.0);
}

// ============================================================
//  Indicator:  In_I-B-1_07  –  Unmet hours for Humidex
// ============================================================

/**
 * @brief Result structure for the Humidex indicator.
 *
 * Comfort bands (Canadian warning system, Borghero et al. [63]):
 * | Band | H range  | Description                      |
 * |------|----------|----------------------------------|
 * |  0   | H < 27   | Little or no discomfort          |
 * |  1   | 27–34    | Noticeable discomfort            |
 * |  2   | 35–39    | Evident discomfort               |
 * |  3   | 40–44    | Intense discomfort / avoid work  |
 * |  4   | 45–54    | Dangerous discomfort             |
 * |  5   | H ≥ 54   | Heat stroke probable             |
 */
struct HumidexResult {
    double band_pct[6] = {};  ///< Percentage of occupied hours in each band [%]
    double unmet_pct   = 0.0; ///< % of occupied hours with H ≥ 27 (any discomfort)
};

/**
 * @brief In_I-B-1_07 – Unmet hours for Humidex (Borghero et al. [63]).
 *
 * Computes the percentage of occupied hours spent in each Humidex comfort band.
 *
 * @param data    Hourly simulation data.
 * @return        HumidexResult with per-band percentages.
 */
inline HumidexResult indicatorHumidex(const std::vector<HourlyData>& data)
{
    // Humidex upper limits for bands 0–4; band 5 = H ≥ 54
    const double limits[5] = {27.0, 35.0, 40.0, 45.0, 54.0};

    HumidexResult result;
    int occ_count = 0;
    int band_count[6] = {};

    for (const auto& d : data) {
        if (!d.occupied) continue;
        ++occ_count;

        const double H = computeHumidex(d.T_air, d.T_dew);
        int band = 5;
        for (int b = 0; b < 5; ++b) {
            if (H < limits[b]) { band = b; break; }
        }
        ++band_count[band];
    }

    if (occ_count == 0) return result;

    int unmet = 0;
    for (int b = 0; b < 6; ++b) {
        result.band_pct[b] = 100.0 * band_count[b] / occ_count;
        if (b >= 1) unmet += band_count[b];
    }
    result.unmet_pct = 100.0 * unmet / occ_count;

    return result;
}

// ============================================================
//  Indicator:  In_I-B-2_03  –  Indoor Overheating Degree (IOD)
// ============================================================

/**
 * @brief In_I-B-2_03 – Indoor Overheating Degree (IOD).
 *
 * Time-integral of the excess operative temperature above the comfort
 * threshold during occupied hours (Borghero et al. [63], Flores-Larsen [34][35]):
 * @code
 *   IOD = Σ max(0, T_op,i − T_comf) · dt   [°C·h]
 * @endcode
 *
 * @param data      Hourly simulation data.
 * @param T_comf    Comfort threshold [°C]  (default 26 °C).
 * @param dt        Time-step duration [h]  (default 1 h).
 * @return          IOD [°C·h].
 */
inline double indicatorIOD(const std::vector<HourlyData>& data,
                           double T_comf = 26.0,
                           double dt     = 1.0)
{
    double iod = 0.0;
    for (const auto& d : data)
        if (d.occupied)
            iod += std::max(0.0, d.T_op - T_comf) * dt;
    return iod;
}

// ============================================================
//  Indicator:  In_I-B-2_11  –  Recovery time
// ============================================================

/**
 * @brief In_I-B-2_11 – Recovery time (Sengupta et al. [79]).
 *
 * Duration from the peak SET instant to the moment when SET drops
 * below the alert threshold and stays below it for at least
 * @p sustain_h hours:
 * @code
 *   t_rec = t_below − t_peak   [h]
 * @endcode
 *
 * @param data       Hourly simulation data.
 * @param SET_alert  Alert threshold [°C]  (default 28 °C).
 * @param sustain_h  Minimum consecutive hours below threshold [h] (default 24 h).
 * @param dt         Time-step duration [h].
 * @return           Recovery time [h];  returns -1 if no exceedance is found.
 */
inline double indicatorRecoveryTime(const std::vector<HourlyData>& data,
                                    double SET_alert = 28.0,
                                    int    sustain_h = 24,
                                    double dt        = 1.0)
{
    const int n = static_cast<int>(data.size());

    // Find index of peak SET
    int i_peak = -1;
    double SET_max = -1e9;
    for (int i = 0; i < n; ++i) {
        if (data[i].SET > SET_max) {
            SET_max = data[i].SET;
            i_peak  = i;
        }
    }
    if (i_peak < 0 || SET_max <= SET_alert) return -1.0;

    // Find first index after peak where SET stays below alert for sustain_h hours
    for (int i = i_peak + 1; i <= n - sustain_h; ++i) {
        bool sustained = true;
        for (int j = i; j < i + sustain_h; ++j) {
            if (data[j].SET >= SET_alert) { sustained = false; break; }
        }
        if (sustained)
            return static_cast<double>(i - i_peak) * dt;
    }

    return -1.0; // did not recover within the dataset
}

// ============================================================
//  Indicator:  In_I-B-2_13  –  Absorptivity time
// ============================================================

/**
 * @brief In_I-B-2_13 – Absorptivity time (Sengupta et al. [79]).
 *
 * Duration from when SET first exceeds the alert threshold to the
 * moment peak SET is reached:
 * @code
 *   t_abs = t_peak − t_start   [h]
 * @endcode
 *
 * Higher values indicate prolonged heat stress → lower thermal resilience.
 *
 * @param data       Hourly simulation data.
 * @param SET_alert  Alert threshold [°C]  (default 28 °C).
 * @param dt         Time-step duration [h].
 * @return           Absorptivity time [h];  returns -1 if SET never exceeds alert.
 */
inline double indicatorAbsorptivityTime(const std::vector<HourlyData>& data,
                                        double SET_alert = 28.0,
                                        double dt        = 1.0)
{
    const int n = static_cast<int>(data.size());

    // Find first exceedance index
    int i_start = -1;
    for (int i = 0; i < n; ++i) {
        if (data[i].SET > SET_alert) { i_start = i; break; }
    }
    if (i_start < 0) return -1.0;

    // Find peak SET index (search from i_start onward)
    int i_peak = i_start;
    for (int i = i_start + 1; i < n; ++i) {
        if (data[i].SET > data[i_peak].SET) i_peak = i;
        // Stop searching if SET drops far below alert before a new peak
        if (data[i].SET < SET_alert - 2.0 && i > i_peak + 24) break;
    }

    return static_cast<double>(i_peak - i_start) * dt;
}

// ============================================================
//  Indicator:  In_O-B-1_01  –  Ambient Warmness Degree (AWD)
// ============================================================

/**
 * @brief In_O-B-1_01 – Ambient Warmness Degree (Flores-Larsen [35]).
 *
 * Sum of positive differences between outdoor air temperature and a
 * base (comfort) temperature over all occupied hours:
 * @code
 *   AWD = Σ max(0, T_out,i − T_b) · dt   [°C·h]
 * @endcode
 *
 * @param data   Hourly simulation data.
 * @param T_base Base temperature [°C]  (default = T_comfort = 26 °C).
 * @param dt     Time-step duration [h].
 * @return       AWD [°C·h].
 */
inline double indicatorAWD(const std::vector<HourlyData>& data,
                           double T_base = 26.0,
                           double dt     = 1.0)
{
    double awd = 0.0;
    for (const auto& d : data)
        if (d.occupied)
            awd += std::max(0.0, d.T_out - T_base) * dt;
    return awd;
}

// ============================================================
//  Indicator:  In_I-B-2_08  –  Overheating Escalation Factor (aIOD)
// ============================================================

/**
 * @brief In_I-B-2_08 – Overheating Escalation Factor (Flores-Larsen [35]).
 *
 * Slope of the linear regression of IOD on AWD across multiple
 * simulation scenarios (e.g. different climatic years or building variants).
 * @code
 *   aIOD = Cov(IOD, AWD) / Var(AWD)
 * @endcode
 *
 * Interpretation:
 *  - aIOD < 1 : building mitigates part of the outdoor thermal stress (resilient)
 *  - aIOD > 1 : building amplifies outdoor overheating indoors (vulnerable)
 *
 * @param iod_values  Vector of IOD values, one per scenario [°C·h].
 * @param awd_values  Vector of AWD values, one per scenario [°C·h].
 * @return            Regression slope aIOD [-].
 * @throws std::invalid_argument if vectors have different sizes or < 2 elements.
 */
inline double indicatorOEF(const std::vector<double>& iod_values,
                           const std::vector<double>& awd_values)
{
    const std::size_t n = iod_values.size();
    if (n < 2 || awd_values.size() != n)
        throw std::invalid_argument("indicatorOEF: need at least 2 matching (IOD, AWD) pairs.");

    const double mean_awd = std::accumulate(awd_values.begin(), awd_values.end(), 0.0) / n;
    const double mean_iod = std::accumulate(iod_values.begin(), iod_values.end(), 0.0) / n;

    double cov = 0.0, var = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double da = awd_values[i] - mean_awd;
        cov += da * (iod_values[i] - mean_iod);
        var += da * da;
    }

    if (var == 0.0)
        throw std::invalid_argument("indicatorOEF: zero variance in AWD – all scenarios identical.");

    return cov / var;
}

// ============================================================
//  Indicator:  In_O-B-2_02  –  Reduction in annual CO₂ emissions
// ============================================================

/**
 * @brief In_O-B-2_02 – Reduction in annual CO₂ emissions (Samuelson et al. [18]).
 *
 * Difference in total annual CO₂ emissions between a reference building
 * and the building under analysis (no explicit equation in the literature):
 * @code
 *   ΔCO₂ = Σ CO₂_ref,i − Σ CO₂_bldg,i   [kg/year]
 * @endcode
 *
 * @param data_ref   Hourly data for the reference (baseline) building.
 * @param data_bldg  Hourly data for the building under analysis.
 * @return           CO₂ reduction [kg/year].  Positive = emissions reduced.
 */
inline double indicatorCO2Reduction(const std::vector<HourlyData>& data_ref,
                                    const std::vector<HourlyData>& data_bldg)
{
    double sum_ref  = 0.0, sum_bldg = 0.0;
    for (const auto& d : data_ref)  sum_ref  += d.CO2_kg;
    for (const auto& d : data_bldg) sum_bldg += d.CO2_kg;
    return sum_ref - sum_bldg;
}

// ============================================================
//  Indicator:  In_O-B-2_03  –  Primary energy consumption
// ============================================================

/**
 * @brief In_O-B-2_03 – Primary energy consumption per unit floor area
 *        (Tomrukcu and Ashrafian [78]).
 *
 * Total primary energy for natural gas and electricity normalised by
 * conditioned floor area:
 * @code
 *   PE = (Σ NG_i · f_gas + Σ EC_i · f_elec) / A_floor   [kWh/m²/year]
 * @endcode
 *
 * Default conversion factors: f_gas = 1.0, f_elec = 2.36 (Turkey grid).
 * Adjust @p params.elec_conv for the specific national grid.
 *
 * @param data    Hourly simulation data.
 * @param params  ResilienceParams (floor_area, elec_conv, gas_conv).
 * @return        Primary energy intensity [kWh/(m²·year)].
 */
inline double indicatorPrimaryEnergy(const std::vector<HourlyData>& data,
                                     const ResilienceParams& params = {})
{
    double total_NG = 0.0, total_EC = 0.0;
    for (const auto& d : data) {
        total_NG += d.NG_kWh;
        total_EC += d.EC_kWh;
    }
    return (total_NG * params.gas_conv + total_EC * params.elec_conv)
           / params.floor_area;
}

// ============================================================
//  Indicator:  In_O-B-3_07  –  Reduction in external surface temperature
// ============================================================

/**
 * @brief In_O-B-3_07 – Reduction in external surface temperature of envelope
 *        (Lassandro and Di Turi [67]).
 *
 * Evaluates UHI mitigation via the reduction of the envelope surface
 * temperature on the hottest day.  No explicit equation is provided in
 * the literature; this implementation computes the difference in mean
 * surface temperature between a reference and a modified envelope:
 * @code
 *   ΔT_surf = mean(T_surf_ref) − mean(T_surf_bldg)   [°C]
 * @endcode
 *
 * @param data_ref   Hourly data for the reference building.
 * @param data_bldg  Hourly data for the building under analysis.
 * @return           Mean surface temperature reduction [°C].
 */
inline double indicatorSurfTempReduction(const std::vector<HourlyData>& data_ref,
                                         const std::vector<HourlyData>& data_bldg)
{
    if (data_ref.empty() || data_bldg.empty()) return 0.0;

    auto mean_surf = [](const std::vector<HourlyData>& v) {
        double s = 0.0;
        for (const auto& d : v) s += d.T_surf;
        return s / static_cast<double>(v.size());
    };

    return mean_surf(data_ref) - mean_surf(data_bldg);
}

// ============================================================
//  Indicator:  In_I-A-1_01  –  Seasonal Building Overheating Index (SBOI)
// ============================================================

/**
 * @brief In_I-A-1_01 – Seasonal Building Overheating Index (Lopez-Garcia et al. [74]).
 *
 * Ratio of the cumulative indoor temperature excess over outdoor temperature
 * to the cumulative outdoor warmth above the base temperature, expressed as
 * a percentage:
 * @code
 *   SBOI = [ Σ max(0, T_op,i − T_out,i) ] / [ Σ max(0, T_out,i − T_b) ]  × 100   [%]
 * @endcode
 *
 * Interpretation:
 *  - SBOI < 100 % : building mitigates outdoor overheating (good resilience)
 *  - SBOI > 100 % : building amplifies indoor overheating (poor resilience)
 *
 * @param data    Hourly simulation data.
 * @param T_base  Base temperature (= comfort temperature) [°C].
 * @return        SBOI [%].
 */
inline double indicatorSBOI(const std::vector<HourlyData>& data,
                            double T_base = 26.0)
{
    double num = 0.0, den = 0.0;
    for (const auto& d : data) {
        if (!d.occupied) continue;
        num += std::max(0.0, d.T_op  - d.T_out);
        den += std::max(0.0, d.T_out - T_base);
    }
    if (den == 0.0) return 0.0;
    return 100.0 * num / den;
}

// ============================================================
//  Indicator:  In_I-A-2_01  –  Resilience Class Index (RCI)
// ============================================================

/**
 * @brief Compute the Weighted Unmet Thermal Performance (WUMTP).
 *
 * WUMTP measures how much the indoor temperature falls below a cold-safety
 * threshold over time (Homaei and Hamdy [73]):
 * @code
 *   WUMTP = Σ max(0, T_cold_safe − T_op,i) · dt   [°C·h]
 * @endcode
 *
 * @param data          Hourly simulation data.
 * @param T_cold_safe   Cold safety threshold [°C]  (default 17.78 °C).
 * @param dt            Time-step duration [h].
 * @return              WUMTP [°C·h].
 */
inline double computeWUMTP(const std::vector<HourlyData>& data,
                           double T_cold_safe = 17.78,
                           double dt          = 1.0)
{
    double wumtp = 0.0;
    for (const auto& d : data)
        wumtp += std::max(0.0, T_cold_safe - d.T_op) * dt;
    return wumtp;
}

/**
 * @brief In_I-A-2_01 – Resilience Class Index (Homaei and Hamdy [73]).
 *
 * Ratio of the WUMTP of the reference (poorly performing) building to that
 * of the building under analysis.  Values > 1 indicate better resilience
 * than the reference:
 * @code
 *   RCI = WUMTP_ref / WUMTP_bldg
 * @endcode
 *
 * Specifically designed for cold-weather / power-outage scenarios.
 *
 * @param data_ref   Hourly data for the reference building.
 * @param data_bldg  Hourly data for the building under analysis.
 * @param params     ResilienceParams (T_cold_safe, dt_h).
 * @return           RCI [-].
 */
inline double indicatorRCI(const std::vector<HourlyData>& data_ref,
                           const std::vector<HourlyData>& data_bldg,
                           const ResilienceParams& params = {})
{
    const double wumtp_ref  = computeWUMTP(data_ref,  params.T_cold_safe, params.dt_h);
    const double wumtp_bldg = computeWUMTP(data_bldg, params.T_cold_safe, params.dt_h);

    if (wumtp_bldg == 0.0) {
        if (wumtp_ref == 0.0) return 1.0; // both buildings always above threshold
        return std::numeric_limits<double>::infinity(); // perfect resilience
    }
    return wumtp_ref / wumtp_bldg;
}

// ============================================================
//  Indicator:  In_I-A-2_02  –  Thermal Resilience Index (TRI)
// ============================================================

/**
 * @brief Compute Whole-building SET-Hours above threshold (WSETH).
 *
 * Accumulated degree-hours above the SET alert threshold for all occupied
 * hours (Ji et al. [75]):
 * @code
 *   WSETH = Σ max(0, SET_i − SET_alert) · dt   [°C·h]
 * @endcode
 *
 * @param data       Hourly simulation data.
 * @param SET_alert  Alert threshold [°C]  (default 28 °C).
 * @param dt         Time-step duration [h].
 * @return           WSETH [°C·h].
 */
inline double computeWSETH(const std::vector<HourlyData>& data,
                           double SET_alert = 28.0,
                           double dt        = 1.0)
{
    double wseth = 0.0;
    for (const auto& d : data)
        if (d.occupied)
            wseth += std::max(0.0, d.SET - SET_alert) * dt;
    return wseth;
}

/**
 * @brief In_I-A-2_02 – Thermal Resilience Index (Ji et al. [75]).
 *
 * Ratio of the WSETH of the original building to that of the retrofitted
 * building.  Values > 1 indicate that the retrofit improved thermal resilience:
 * @code
 *   TRI = WSETH_original / WSETH_retrofitted
 * @endcode
 *
 * Specifically designed for summer heatwave conditions.
 *
 * @param data_orig  Hourly data for the original (unretrofitted) building.
 * @param data_retr  Hourly data for the retrofitted building.
 * @param params     ResilienceParams (T_SET_alert, dt_h).
 * @return           TRI [-].
 */
inline double indicatorTRI(const std::vector<HourlyData>& data_orig,
                           const std::vector<HourlyData>& data_retr,
                           const ResilienceParams& params = {})
{
    const double wseth_orig = computeWSETH(data_orig, params.T_SET_alert, params.dt_h);
    const double wseth_retr = computeWSETH(data_retr, params.T_SET_alert, params.dt_h);

    if (wseth_retr == 0.0) {
        if (wseth_orig == 0.0) return 1.0;
        return std::numeric_limits<double>::infinity();
    }
    return wseth_orig / wseth_retr;
}

// ============================================================
//  Result structures
// ============================================================

/**
 * @brief All single-dataset resilience indicator results.
 *
 * Populated by computeAllIndicators().
 * Fields use the same units as the underlying indicator functions.
 */
struct SingleDatasetResults {
    // In_I-B-1_07 - Humidex unmet hours
    HumidexResult humidex;

    // In_I-B-2_03 - Indoor Overheating Degree [deg_C*h]
    double IOD = 0.0;

    // In_I-B-2_11 - Recovery time [h];  -1 = SET never exceeded alert or never recovered
    double recovery_time_h = -1.0;

    // In_I-B-2_13 - Absorptivity time [h];  -1 = SET never exceeded alert
    double absorptivity_time_h = -1.0;

    // In_O-B-1_01 - Ambient Warmness Degree [deg_C*h]
    double AWD = 0.0;

    // In_O-B-2_03 - Primary energy consumption [kWh/(m2*year)]
    double primary_energy = 0.0;

    // In_I-A-1_01 - Seasonal Building Overheating Index [%]
    double SBOI = 0.0;
};

/**
 * @brief All two-dataset resilience indicator results.
 *
 * Populated by computeTwoDatasetIndicators().
 * Requires a reference dataset and the building dataset.
 */
struct TwoDatasetResults {
    // In_O-B-2_02 - CO2 reduction (ref - bldg) [kg/year]; positive = improvement
    double CO2_reduction = 0.0;

    // In_O-B-3_07 - External surface temperature reduction (ref - bldg) [deg_C]
    double surf_temp_reduction = 0.0;

    // In_I-A-2_01 - Resilience Class Index [-]; > 1 = more resilient than reference
    double RCI = 0.0;

    // In_I-A-2_02 - Thermal Resilience Index [-]; > 1 = retrofit improved resilience
    double TRI = 0.0;

    // In_I-B-2_08 - Overheating Escalation Factor [-]; 0 if not computed
    double OEF = 0.0;
};

// ============================================================
//  Aggregation functions
// ============================================================

/**
 * @brief Compute all single-dataset resilience indicators in one call.
 *
 * Requires that computeDerivedFields() has already been called on @p data.
 *
 * @param data    Hourly simulation data (with derived fields populated).
 * @param params  Resilience parameters (thresholds, floor area, etc.).
 * @return        Populated SingleDatasetResults.
 */
inline SingleDatasetResults computeAllIndicators(const std::vector<HourlyData>& data,
                                                 const ResilienceParams& params = {})
{
    SingleDatasetResults r;
    r.humidex              = indicatorHumidex(data);
    r.IOD                  = indicatorIOD(data, params.T_comfort, params.dt_h);
    r.recovery_time_h      = indicatorRecoveryTime(data, params.T_SET_alert, 24, params.dt_h);
    r.absorptivity_time_h  = indicatorAbsorptivityTime(data, params.T_SET_alert, params.dt_h);
    r.AWD                  = indicatorAWD(data, params.T_base, params.dt_h);
    r.primary_energy       = indicatorPrimaryEnergy(data, params);
    r.SBOI                 = indicatorSBOI(data, params.T_base);
    return r;
}

/**
 * @brief Compute all two-dataset resilience indicators in one call.
 *
 * Requires that computeDerivedFields() has already been called on both datasets.
 *
 * The Overheating Escalation Factor (OEF / aIOD) is computed from the
 * optional @p iod_scenarios / @p awd_scenarios vectors (one value per
 * climate scenario or building variant).  If either vector is empty the
 * OEF field is left at 0 and no exception is thrown.
 *
 * @param data_ref      Reference (baseline) building hourly data.
 * @param data_bldg     Building under analysis hourly data.
 * @param params        Resilience parameters.
 * @param iod_scenarios Per-scenario IOD values [deg_C*h]  (optional, for OEF).
 * @param awd_scenarios Per-scenario AWD values [deg_C*h]  (optional, for OEF).
 * @return              Populated TwoDatasetResults.
 */
inline TwoDatasetResults computeTwoDatasetIndicators(
        const std::vector<HourlyData>& data_ref,
        const std::vector<HourlyData>& data_bldg,
        const ResilienceParams& params = {},
        const std::vector<double>& iod_scenarios = {},
        const std::vector<double>& awd_scenarios = {})
{
    TwoDatasetResults r;
    r.CO2_reduction       = indicatorCO2Reduction(data_ref, data_bldg);
    r.surf_temp_reduction = indicatorSurfTempReduction(data_ref, data_bldg);
    r.RCI                 = indicatorRCI(data_ref, data_bldg, params);
    r.TRI                 = indicatorTRI(data_ref, data_bldg, params);
    if (!iod_scenarios.empty() && iod_scenarios.size() == awd_scenarios.size())
        r.OEF = indicatorOEF(iod_scenarios, awd_scenarios);
    return r;
}
