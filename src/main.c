#include "ww_game.h"

#include "ww_common.h"

#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ww_usage(void)
{
    puts("Wacky Wheels 95 reconstruction\n"
         "  --self-test          validate the registered WACKY.DAT\n"
         "  --smoke-test         initialize SDL, draw three frames, and exit\n"
         "  --race-smoke N       render three bring-up frames for track 1..30\n"
         "  --duck-smoke N       render Duck Shoot frames for track 6..11\n"
         "  --asset NAME.PCX     display a named PCX from the archive\n"
         "  --data PATH          use an explicit WACKY.DAT path\n");
}

static bool ww_file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static const char *ww_default_data_path(char path[1024])
{
    char *base;
    if (ww_file_exists("WACKY.DAT")) {
        return "WACKY.DAT";
    }
    base = SDL_GetBasePath();
    if (base != NULL) {
        int written = snprintf(path, 1024, "%sWACKY.DAT", base);
        SDL_free(base);
        if (written > 0 && written < 1024 && ww_file_exists(path)) {
            return path;
        }
    }
    return "WACKY.DAT";
}

int main(int argc, char **argv)
{
    WwGameOptions options;
    WwGame game;
    char default_path[1024];
    int i;
    int result;

    SDL_SetMainReady();
    memset(&options, 0, sizeof(options));
    options.data_path = ww_default_data_path(default_path);
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) {
            options.self_test = true;
        } else if (strcmp(argv[i], "--smoke-test") == 0) {
            options.smoke_frames = 3;
        } else if (strcmp(argv[i], "--race-smoke") == 0 && i + 1 < argc) {
            char *end;
            unsigned long track;
            errno = 0;
            track = strtoul(argv[++i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' ||
                track < WW_RACE_FIRST_TRACK || track > WW_RACE_LAST_TRACK) {
                ww_error("--race-smoke requires a track from 1 through 30");
                return 2;
            }
            options.race_smoke_track = (unsigned)track;
            options.smoke_frames = 3;
        } else if (strcmp(argv[i], "--duck-smoke") == 0 && i + 1 < argc) {
            char *end;
            unsigned long track;
            errno = 0;
            track = strtoul(argv[++i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' ||
                track < WW_DUCK_TRACK_FIRST || track > WW_DUCK_TRACK_LAST) {
                ww_error("--duck-smoke requires a track from 6 through 11");
                return 2;
            }
            options.duck_smoke_track = (unsigned)track;
            options.smoke_frames = 3;
        } else if (strcmp(argv[i], "--asset") == 0 && i + 1 < argc) {
            options.asset_name = argv[++i];
        } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            options.data_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            ww_usage();
            return 0;
        } else {
            ww_error("unknown argument: %s", argv[i]);
            ww_usage();
            return 2;
        }
    }

    if (options.self_test) {
        return ww_game_self_test(options.data_path) ? 0 : 1;
    }
    if (!ww_game_open(&game, &options)) {
        return 1;
    }
    result = ww_game_run(&game);
    ww_game_close(&game);
    return result;
}
