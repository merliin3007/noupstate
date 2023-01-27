/**
 * NOUPSTATE - simple utility tool for changing nouveau pstates.
 * by https://github.com/merliin3007
 *
 * USE AT YOUR OWN RISK!
 */

/* build options */
//#define DEBUG         /* uncomment to build in debug mode */
#define ENABLE_COLORS   /* remove to disable colors */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#define PROGRAM_NAME    "noupstate"
#define AUTHOR          "merliin3007"

#define VERSION_MAJOR   1
#define VERSION_MINOR   0

#define xstr(s) str(s)
#define str(s) #s

const char INFO[] = 
    PROGRAM_NAME " v" xstr(VERSION_MAJOR) "." xstr(VERSION_MINOR) " by " AUTHOR ".\n"
    "simple utility tool for changing nouveau pstates.\n"
    "needs elevated permissions.\n"
    "USE AT YOUR OWN RISK!";
const char HELP[] =
    "list  -> list all avaliable pstates.\n"
    "   -d -> device id, defaults to 0\n"
    "set   -> set a specific pstate.\n"
    "   -d -> device id, defaults to 0\n"
    "   -p -> performance | save-energy | id:<id> | val:<value>\n"
    "         if no profile is specified,\n"
    "         you will be prompted with a list\n"
    "         to choose from.";

const char PSTATES_PATH_FSTR[] = "/sys/kernel/debug/dri/%d/pstate";

#define PATH_MAXSZ  64

/* terminal colors */
#ifdef ENABLE_COLORS
    #define BLK "\033[0;30m"
    #define RED "\033[0;31m"
    #define GRN "\033[0;32m"
    #define YEL "\033[0;33m"
    #define BLU "\033[0;34m"
    #define MAG "\033[0;35m"
    #define CYN "\033[0;36m"
    #define WHT "\033[0;37m"
    #define NC "\033[0m"
#else
    #define BLK ""
    #define RED ""
    #define GRN ""
    #define YEL ""
    #define BLU ""
    #define MAG ""
    #define CYN ""
    #define WHT ""
    #define NC ""
#endif /* ENABLE_COLORS */

#ifdef DEBUG
    /**
     * @brief log to a file.
     *
     * @param file out file.
     * @param prefix log message prefix.
     * @param srcfile source file from where the log message originates from.
     * @param line the line of the source file where the log message originates from.
     * @param function function from which the log message originates.
     * @param fstr format string of the log message.
     * @param ... format string arguments
     */
    void flog_(FILE *file, const char *prefix, const char *srcfile, 
          long line, const char *function, const char *fstr, ...)
    {
        va_list vargs;
        fprintf(file, "[%s" NC "] %s:%ld in function %s -> ", prefix, srcfile, line, function);
        va_start(vargs, fstr);
        vfprintf(file, fstr, vargs);
        fputs("\n", file);
        va_end(vargs);
    }

    #define flog(file, prefix, ...) \
        flog_(file, prefix, __FILE__, __LINE__, __func__, __VA_ARGS__)

#else
    /**
     * @brief log to a file.
     *
     * @param file out file.
     * @param prefix log message prefix.
     * @param fstr format string of the log message.
     * @param ... format string arguments
     */
    void flog_(FILE *file, const char *prefix, const char *fstr, ...)
    {
        va_list vargs;
        fprintf(file, "[%s" NC "] -> ", prefix);
        va_start(vargs, fstr);
        vfprintf(file, fstr, vargs);
        fputs("\n", file);
        va_end(vargs);
    }

    #define flog(file, prefix, ...) \
        flog_(file, prefix, __VA_ARGS__)

#endif /* DEBUG */

#define log_err(...) \
    flog(stderr, RED "error", __VA_ARGS__)

#define log_warn(...) \
    flog(stdout, YEL "warning", __VA_ARGS__)

#define log_succ(...) \
    flog(stdout, GRN "success", __VA_ARGS__)

#define log(...) \
    flog(stdout, "log", __VA_ARGS__)

/**
 * @brief information about a pstate
 */
struct PStateInfo {
    unsigned int pstate;
    int core_minclock;
    int core_maxclock;
    int mem_clock;
};

/**
 * @brief contains information about all avaliable pstates
 */
struct PStates {
    struct PStateInfo *pstates;
    size_t pstates_len;
    size_t perf, enrgy;
};

/**
 * @brief sets a new pstate.
 *
 * @param card the device to set the pstate for.
 * @param pstate the new pstate.
 * @return ret -> 0 on success; 1 otherwise.
 */
int write_pstate(int card, unsigned int pstate)
{
    int ret = 0;

    /* path */
    char path[PATH_MAXSZ];
    int err = snprintf(path, PATH_MAXSZ - 1, PSTATES_PATH_FSTR, card);
    path[PATH_MAXSZ - 1] = '\0';
    if (err <= 0) {
        log_err("can not format path string.");
        return 1;
    }

    /* open file */
    int fd  = open(path, O_WRONLY);
    if (fd < 0) {
        log_err("can not open '%s'.", path);
        goto err;
    }

    /* format value */
    char pstate_str[16];
    int pstate_str_len = snprintf(pstate_str, 16, "%02x", pstate);
    if (pstate_str_len <= 0) {
        log_err("can not format pstate.");
        goto err;
    }

    /* write value */
    pstate_str_len += 1;
    if ((write(fd, pstate_str, pstate_str_len)) < pstate_str_len) {
        log_err("can not write pstate.");
        goto err;
    }

cleanup:
    close(fd);
    if (ret == 0) {
        log_succ("pstate successfully changed to %02x.", pstate);
    } else {
        log_err("failed to change pstate to %02x.", pstate);
    }
    return ret;

err:
    ret = 1;
    goto cleanup;
}

/**
 * @brief parse information about a pstate from string.
 *
 * @param line the string line containing information about a pstate.
 * @param pstateinfo PStateInfo struct to store the parsed results in.
 * @return 0 on success; 1 otherwise.
 */
int parse_pstate(char *line, struct PStateInfo *pstateinfo)
{
    char *ptr;
    pstateinfo->pstate = (unsigned int)strtoul(line, &ptr, 16);

    pstateinfo->core_minclock   = -1;
    pstateinfo->core_maxclock   = -1;
    pstateinfo->mem_clock       = -1;

    ptr = strtok(ptr, " \n");
    while (ptr != NULL) {
        if (strcmp(ptr, "core") == 0 || strcmp(ptr, "memory") == 0) {
            bool core = strcmp(ptr, "core") == 0;
            int minclock = 0, maxclock = 0;
            if ((ptr = strtok(NULL, " \n")) == NULL) {
                log_err("pstates parsing: expected more tokens.");
                return 1;
            } else {
                minclock = (int)strtol(ptr, &ptr, 10);
            }

            if (*ptr == '-') {
                ptr++;
                maxclock = (int)strtol(ptr, &ptr, 10);
            } else {
                maxclock = minclock;
            }

            if (*ptr != '\0') {
                log_err("pstates parsing: expected end of token.");
                return 1;
            }

            if ((ptr = strtok(NULL, " \n")) == NULL) {
                log_err("pstates parsing: expected clock unit.");
                return 1;
            } else {
                if (strcmp(ptr, "GHz") == 0) {
                    maxclock *= 1000;
                    minclock *= 1000;
                } else if (strcmp(ptr, "MHz") != 0) {
                    log_err("pstates parsing: unkown clock unit: '%s'.", ptr);
                    return 1;
                }
            }

            if (core) {
                pstateinfo->core_minclock = minclock;
                pstateinfo->core_maxclock = maxclock;
            } else {
                pstateinfo->mem_clock = maxclock;
            }
        }
        ptr = strtok(NULL, " ");
    }

    return 0;
}

/**
 * @brief read pstates for a certain device.
 *
 * @param card the device to read the avaliable pstates for.
 * @param pstates PStates struct to store the results in.
 * @return 0 on success; 1 otherwise.
 */
int read_pstates(int card, struct PStates *pstates)
{
    /* format path */
    char path[PATH_MAXSZ];
    int err = snprintf(path, PATH_MAXSZ - 1, PSTATES_PATH_FSTR, card);
    path[PATH_MAXSZ - 1] = '\0';
    if (err <= 0) {
        log_err("can't format pstates-path.");
        return 1;
    }

    pstates->pstates_len = 0;
    size_t pstates_allocated = 8;
    pstates->pstates = malloc(sizeof(*pstates->pstates) * pstates_allocated);
    if (pstates->pstates == NULL) {
        log_err("can not allocte memory.");
        return 1;
    }

    /* read pstates file */
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        log_err("can not open file.");
        perror("");
        return 1;
    }
    char *buf = NULL;
    size_t bufsz;
    while (getline(&buf, &bufsz, f) > 0) {
        if (pstates->pstates_len >= pstates_allocated) {
            pstates->pstates_len *= 1.5;
            struct PStateInfo *newpstate = realloc(pstates->pstates, pstates->pstates_len);
            if (newpstate == NULL) {
                log_err("can not reallocate memory.");
                free(pstates->pstates);
                return 1;
            }
        }
        if (!parse_pstate(buf, pstates->pstates + pstates->pstates_len)) {
            pstates->pstates_len++;
        }
    }

    /* get performance and save-energy profiles */
    pstates->perf = 0, pstates->enrgy = 0;
    for (size_t i = 0; i < pstates->pstates_len; ++i) {
        if (pstates->pstates[i].pstate == 0xAC) {
            continue;
        } else if (pstates->pstates[i].core_maxclock >= pstates->pstates[pstates->perf].core_maxclock) {
            pstates->perf = i;
        } else if (pstates->pstates[i].core_maxclock < pstates->pstates[pstates->enrgy].core_maxclock) {
            pstates->enrgy = i;
        }
    }

    return 0;
}

/**
 * @brief prints pstates
 *
 * @param pstates the pstates to print lul.
 */
void print_pstates(struct PStates *pstates)
{
    for (size_t i = 0; i < pstates->pstates_len; ++i) {
        printf("[id:%zu] val:%02x, core-clock %d-%d MHz, memory-clock %d MHz",
               i, pstates->pstates[i].pstate, pstates->pstates[i].core_minclock,
               pstates->pstates[i].core_maxclock, pstates->pstates[i].mem_clock);
        if (i == pstates->perf) {
            puts(" (performance profile)");
        } else if (i == pstates->enrgy) {
            puts(" (save-energy profile)");
        } else if (pstates->pstates[i].pstate == 0xAC) {
            puts(" (DON'T USE!)");
        } else {
            puts("");
        }
    }
}

enum PStateType {
    PT_NONE             = 0x0,
    PT_PROFILE_PERF     = 0x1,
    PT_PROFILE_ENRGY    = 0x2,
    PT_ID               = 0x3,
    PT_VALUE            = 0x4
};

#define arg_get_val(argv, argc, i, val, err_msg)    \
    if (argv[i][2] != '\0') {                       \
        val = argv[i] + 2;                          \
    } else if (argc > i + 1) {                      \
        ++i;                                        \
        val = argv[i];                              \
    } else {                                        \
        log_err(err_msg);                           \
        exit(EXIT_FAILURE);                         \
    }

enum Operation {
    OP_NONE             = 0x0,
    OP_SET              = 0x1,
    OP_LIST             = 0x2
};

/**
 * @brief check if string begins with a prefix.
 *
 * @param prefix the prefix.
 * @param str the string to check for the prefix.
 * @return true if str begins with prefix; false otherwise.
 */
bool strprefix(const char *prefix, const char *str)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

int main(int argc, char **argv)
{
    puts(INFO);
    puts("");

    enum Operation op               = OP_NONE;

    int device                      = 0;
    enum PStateType new_pstate_type = PT_NONE;
    unsigned int new_pstate         = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            char *val;
            switch(argv[i][1]) {
            case 'h':
                puts(HELP);
                exit(EXIT_SUCCESS);
                break;
            case 'd':
                arg_get_val(argv, argc, i, val, "expected device id");
                device = atoi(val);
                break;
            case 'p':
                arg_get_val(argv, argc, i, val,
                            "expected pstate profile-name, value or id");
                if (strcmp("performance", val) == 0) {
                    new_pstate_type = PT_PROFILE_PERF;
                } else if (strcmp("save-energy", val) == 0) {
                    new_pstate_type = PT_PROFILE_ENRGY;
                } else if (strprefix("id:", val)) {
                    new_pstate_type = PT_ID;
                    new_pstate = atoi(val + 3);
                } else if (strprefix("val:", val)) {
                    new_pstate_type = PT_VALUE;
                    new_pstate = (int)strtol(val + 4, NULL, 16);
                } else {
                    log_err("unknown pstate type: '%s'", val);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                log_err("unkown option: '-%s'", argv[i]);
                exit(EXIT_FAILURE);
                break;
            }
        } else if (strcmp("set", argv[i]) == 0) {
            if (op != OP_NONE) {
                puts(HELP);
                exit(EXIT_FAILURE);
            }
            op = OP_SET;
        } else if (strcmp("list", argv[i]) == 0) {
            if (op != OP_NONE) {
                puts(HELP);
                exit(EXIT_FAILURE);
            }
            op = OP_LIST;
        }
    }

    struct PStates pstates;
    if (op != OP_NONE) {
        if ((read_pstates(device, &pstates))) {
            exit(EXIT_FAILURE);
        } else if (pstates.pstates_len == 0) {
            log_err("no pstates avaliable for device %d.", device);
            exit(EXIT_FAILURE);
        }
    }

    switch(op) {
    case OP_NONE:
        puts(HELP);
        break;
    case OP_SET:
        if (new_pstate_type == PT_VALUE) {
            bool is_known_pstate = false;
            /* check if pstate exists */
            for (size_t i = 0; i < pstates.pstates_len; ++i) {
                if (pstates.pstates[i].pstate == new_pstate) {
                    is_known_pstate = true;
                    break;
                }
            }
            /* warn about unkown pstate */
            if (!is_known_pstate) {
                log_warn("Unkown pstate: '%02x'. Continue? [y/N]", new_pstate);
                char c = fgetc(stdin);
                if (c != 'y' && c != 'Y') {
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (new_pstate_type == PT_NONE) {
            print_pstates(&pstates);
            printf("Type [0-%zu].\n> ", pstates.pstates_len - 1);
            fflush(stdout);
            if ((scanf("%u", &new_pstate)) != 1) {
                fputs("invalid input.\n", stderr);
                exit(EXIT_FAILURE);
            } else if ((size_t)new_pstate >= pstates.pstates_len) {
                fputs("invalid input.\n", stderr);
                exit(EXIT_FAILURE);
            }
            new_pstate_type = PT_ID;
        }

        if (new_pstate_type == PT_PROFILE_PERF) {
            new_pstate = pstates.pstates[pstates.perf].pstate;
        } else if (new_pstate_type == PT_PROFILE_ENRGY) {
            new_pstate = pstates.pstates[pstates.enrgy].pstate;
        }

        if (new_pstate_type == PT_ID) {
            new_pstate = pstates.pstates[new_pstate].pstate;
        }

        write_pstate(device, new_pstate);
        break;
    case OP_LIST:
        print_pstates(&pstates);
        break;
    default:
        /* this should never happen */
        log_err("unkown operation.");
        break;
    }
}

