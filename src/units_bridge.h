/*
 * units_bridge.h
 * Thin C bridge for gnu-units.  Declares a safe, minimal API that can be
 * included from C++ without touching files inside the gnu-units submodule.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialise the units database.
 * units_dat_path – absolute path to units.dat; pass NULL to search PATH.
 * Returns 0 on success.
 */
int bridge_init(const char *units_dat_path);

/*
 * Convert value from unit_a to unit_b.
 * If unit_b is NULL/empty, result is reduced to SI base units and the
 * base-unit string is written into out_si_unit.
 *
 * Returns 0 on success, non-zero on failure (error text in out_err):
 *   1 – unknown source unit
 *   2 – unknown target unit
 *   3 – incompatible dimensions
 *   4 – reduction error
 */
int bridge_convert(double      value,
                   const char *unit_a,
                   const char *unit_b,
                   double     *out_value,
                   char       *out_si_unit,
                   int         si_unit_len,
                   char       *out_err,
                   int         err_len);

#ifdef __cplusplus
}
#endif
