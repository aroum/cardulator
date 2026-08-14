/*
 * units_bridge.c  –  compiled as pure C so gnu-units/units.h K&R
 * declarations do not conflict with C++ system headers.
 * This file lives in the main cpp directory, not inside the gnu-units submodule.
 */

/* Standard headers must come first so units.h #ifndef guards fire correctly. */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Suppress old-style K&R re-declarations that live inside units.h. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
#include "units.h"
#pragma clang diagnostic pop

#include "units_bridge.h"

/* Functions defined in units.c but not declared in units.h. */
extern int   completereduce(struct unittype *unit);
extern int   readunits(char *file, FILE *errfile,
                       int *unitcount, int *prefixcount, int *funccount,
                       int depth);
extern char *findunitsfile(void);
extern int   unit2num(struct unittype *input);
/* mylocale is normally set by units.c main(); init before readunits(). */
extern char *mylocale;

static int g_initialized = 0;

int bridge_init(const char *units_dat_path)
{
    if (g_initialized) return 0;

    if (!mylocale) {
        mylocale = getenv("LOCALE");
        if (!mylocale) mylocale = "en_US";
    }

    int ucount = 0, pcount = 0, fcount = 0;
    int err;
    if (units_dat_path) {
        err = readunits((char *)units_dat_path, stderr, &ucount, &pcount, &fcount, 0);
    } else {
        char *path = findunitsfile();
        err = readunits(path, stderr, &ucount, &pcount, &fcount, 0);
    }

    if (err == 0 || err == E_UNKNOWNUNIT) {
        g_initialized = 1;
        return 0;
    }
    return err;
}

int bridge_convert(double      value,
                   const char *unit_a,
                   const char *unit_b,
                   double     *out_value,
                   char       *out_si_unit,
                   int         si_unit_len,
                   char       *out_err,
                   int         err_len)
{
    struct unittype have_unit, want_unit;
    char *errmsg = NULL;
    int   errloc = 0;
    int   err;

    if (out_err     && err_len    > 0) out_err[0]    = '\0';
    if (out_si_unit && si_unit_len > 0) out_si_unit[0] = '\0';

    initializeunit(&have_unit);
    initializeunit(&want_unit);

    /* Parse source unit. */
    char have_str[256];
    snprintf(have_str, sizeof(have_str), "%s", unit_a);
    err = parseunit(&have_unit, have_str, &errmsg, &errloc);
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len, "Unknown unit: %s", unit_a);
        freeunit(&have_unit); freeunit(&want_unit);
        return 1;
    }

    have_unit.factor *= value;

    err = completereduce(&have_unit);
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len, "Reduce error: %s", unit_a);
        freeunit(&have_unit); freeunit(&want_unit);
        return 4;
    }

    /* SI output — no target unit. */
    if (!unit_b || unit_b[0] == '\0') {
        *out_value = have_unit.factor;
        if (out_si_unit && si_unit_len > 0) {
            out_si_unit[0] = '\0';
            int first = 1;
            for (int i = 0; have_unit.numerator[i]; i++) {
                if (have_unit.numerator[i][0] == '\0') continue;
                if (!first) strncat(out_si_unit, " ", (size_t)si_unit_len - strlen(out_si_unit) - 1);
                strncat(out_si_unit, have_unit.numerator[i], (size_t)si_unit_len - strlen(out_si_unit) - 1);
                first = 0;
            }
            int has_den = 0;
            for (int i = 0; have_unit.denominator[i]; i++)
                if (have_unit.denominator[i][0] != '\0') { has_den = 1; break; }
            if (has_den) {
                strncat(out_si_unit, " / ", (size_t)si_unit_len - strlen(out_si_unit) - 1);
                first = 1;
                for (int i = 0; have_unit.denominator[i]; i++) {
                    if (have_unit.denominator[i][0] == '\0') continue;
                    if (!first) strncat(out_si_unit, " ", (size_t)si_unit_len - strlen(out_si_unit) - 1);
                    strncat(out_si_unit, have_unit.denominator[i], (size_t)si_unit_len - strlen(out_si_unit) - 1);
                    first = 0;
                }
            }
        }
        freeunit(&have_unit); freeunit(&want_unit);
        return 0;
    }

    /* Parse target unit. */
    char want_str[256];
    snprintf(want_str, sizeof(want_str), "%s", unit_b);
    err = parseunit(&want_unit, want_str, &errmsg, &errloc);
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len, "Unknown unit: %s", unit_b);
        freeunit(&have_unit); freeunit(&want_unit);
        return 2;
    }

    err = completereduce(&want_unit);
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len, "Reduce error: %s", unit_b);
        freeunit(&have_unit); freeunit(&want_unit);
        return 4;
    }

    /* Divide have/want and check dimensions cancel. */
    struct unittype diff;
    initializeunit(&diff);
    unitcopy(&diff, &have_unit);
    err = divunit(&diff, &want_unit);
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len, "Division error");
        freeunit(&have_unit); freeunit(&want_unit); freeunit(&diff);
        return 3;
    }

    double ratio = diff.factor;
    err = unit2num(&diff);   /* also calls completereduce; frees diff on success */
    if (err) {
        if (out_err) snprintf(out_err, (size_t)err_len,
                              "Incompatible units: %s and %s", unit_a, unit_b);
        freeunit(&have_unit); freeunit(&want_unit);
        return 3;
    }

    *out_value = ratio;
    freeunit(&have_unit); freeunit(&want_unit);
    return 0;
}
