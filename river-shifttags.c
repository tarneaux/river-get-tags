#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "river-control-unstable-v1.h"
#include "river-status-unstable-v1.h"

struct wl_display* wl_display = NULL;
struct wl_output* wl_output = NULL;
struct wl_seat* seat = NULL;

struct zriver_status_manager_v1* river_status_manager = NULL;
struct zriver_control_v1* river_controller = NULL;

struct zriver_output_status_v1* river_output_status = NULL;
struct zriver_seat_status_v1* river_seat_status = NULL;

/* Passed Configuration */
int shifts = 1;
int num_tags = 9;
int start_tag = 0;
bool occupied_only = false;

/* Globals */
uint32_t occupied_tags = 0;
uint32_t init_tags;

static void
noop()
{
}

const char usage[] =
  "Usage: river-shiftview [OPTIONS]\n"
  "   --shifts VALUE     value by which to rotate selected tag.\n"
  "                      (default: 1)\n"
  "   --num-tags VALUE   number of tags to rotate\n"
  "                      (default: 9)\n"
  "   --start VALUE      First tag to rotate from. (starts with 0)\n"
  "                      (default: 0)\n"
  "   --occupied         Skip unoccupied tags.\n"
  "   --help             print this message and exit.\n"
  "\n"
  "\n";

uint32_t
get_update_mask(int start_tag, int num_tags)
{
    return ~(num_tags < (int)sizeof(init_tags) * CHAR_BIT ? (~(0U) << num_tags)
                                                          : 0U)
           << start_tag;
}
/*
 * Main rotation algo (Lots of bitwise logic)
 */
uint32_t
rotate(uint32_t init_tags, int shifts, int start_tag, int num_tags)
{
    shifts %= num_tags;

    /* using ternary operator because shifting more than CHAR_BIT size is UB */
    const unsigned int update_mask = get_update_mask(start_tag, num_tags);

    uint32_t rotated = init_tags & update_mask;

    if (shifts < 1) {
        shifts = -shifts;
        rotated = (rotated >> shifts) | (rotated << (num_tags - shifts));
    } else {
        rotated = (rotated << shifts) | (rotated >> (num_tags - shifts));
    }

    return (update_mask & rotated) | (~update_mask & init_tags);
}

uint32_t
snap_to_occupied(uint32_t new_tagmask,
                 uint32_t occupied_tags,
                 int start_tag,
                 int num_tags,
                 int shifts)
{
    const uint32_t update_mask = get_update_mask(start_tag, num_tags);
    uint32_t final_tagmask = 0;

    for (uint32_t i = 1U << start_tag;
         i <= 1U << (start_tag + num_tags - 1) && i != 0;
         i <<= 1) {

        if (!(i & new_tagmask)) {
            /* Not focused in new tagmask */
            continue;
        }

        if (i & occupied_tags) {
            /* tag is already occupied */
            final_tagmask |= i;
            continue;
        }

        /*
         * Search for the closest occupied tag towards direction of shift
         */
        if (shifts > 0) {
            for (uint32_t j = i; j <= 1U << (start_tag + num_tags); j <<= 1) {
                if (j & occupied_tags) {
                    final_tagmask |= j;
                    goto ENDLOOP;
                }
            }
            for (uint32_t j = 1 << start_tag; j <= i; j <<= 1) {
                if (j & occupied_tags) {
                    final_tagmask |= j;
                    goto ENDLOOP;
                }
            }
        } else {
            for (uint32_t j = i; j <= i; j >>= 1) {
                if (j & occupied_tags) {
                    final_tagmask |= j;
                    goto ENDLOOP;
                }
            }
            for (uint32_t j = 1 << start_tag; j <= 1U << (start_tag + num_tags);
                 j >>= 1) {
                if (j & occupied_tags) {
                    final_tagmask |= j;
                    goto ENDLOOP;
                }
            }
        }
    ENDLOOP:;
    }

    return (update_mask & final_tagmask) | (~update_mask & new_tagmask);
}

static void
set_tagmask(struct zriver_control_v1* river_controller, uint32_t new_tagmask)
{
    char* tags_str = malloc((size_t)snprintf(NULL, 0, "%u", new_tagmask));
    sprintf(tags_str, "%d", new_tagmask);

    zriver_control_v1_add_argument(river_controller, "set-focused-tags");
    zriver_control_v1_add_argument(river_controller, tags_str);
    zriver_control_v1_run_command(river_controller, seat);
    wl_display_flush(wl_display);
}

static void
set_occupied_tags(void* data,
                  struct zriver_output_status_v1* river_status,
                  struct wl_array* tags)
{
    uint32_t* i;
    wl_array_for_each(i, tags)
    {
        occupied_tags |= *i;
    }
}

static void
set_init_tagmask(void* data,
                 struct zriver_output_status_v1* output_status_v1,
                 unsigned int tagmask)
{
    init_tags = tagmask;
}

static const struct zriver_output_status_v1_listener output_status_listener = {
    .focused_tags = set_init_tagmask,
    .view_tags = set_occupied_tags,
    .urgent_tags = noop,
};

static void
river_seat_status_handle_focused_output(
  void* data,
  struct zriver_seat_status_v1* seat_status,
  struct wl_output* focused_output)
{
    river_output_status = zriver_status_manager_v1_get_river_output_status(
      river_status_manager, focused_output);

    zriver_output_status_v1_add_listener(
      river_output_status, &output_status_listener, NULL);
    wl_display_roundtrip(wl_display);
}

static const struct zriver_seat_status_v1_listener
  river_seat_status_listener = {
      .focused_output = river_seat_status_handle_focused_output,
      .unfocused_output = noop,
      .focused_view = noop,
  };

static void
global_registry_handler(void* data,
                        struct wl_registry* registry,
                        uint32_t id,
                        const char* interface,
                        uint32_t version)
{
    /*printf("Got a registry event for %s id %d\n", interface, id);*/
    if (strcmp(interface, "wl_output") == 0) {
        wl_registry_bind(registry, id, &wl_output_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        seat = wl_registry_bind(registry, id, &wl_seat_interface, 3);
    } else if (strcmp(interface, zriver_status_manager_v1_interface.name) ==
               0) {
        river_status_manager = wl_registry_bind(
          registry, id, &zriver_status_manager_v1_interface, 2);
    } else if (strcmp(interface, zriver_control_v1_interface.name) == 0) {
        river_controller =
          wl_registry_bind(registry, id, &zriver_control_v1_interface, 1);
    }
}

static void
global_registry_remover(void* data, struct wl_registry* registry, uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

int
main(int argc, char* argv[])
{
    /*
     * Parse options and parameters
     */
    enum { HELP, SHIFT_VALUE, START_TAG, NUM_TAGS, OCCUPIED };
    static struct option opts[] = {
        { "help", no_argument, NULL, HELP },
        { "occupied", no_argument, NULL, OCCUPIED },
        { "shifts", required_argument, NULL, SHIFT_VALUE },
        { "num-tags", required_argument, NULL, NUM_TAGS },
        { "start", required_argument, NULL, START_TAG },
        { NULL, 0, NULL, 0 },
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch (opt) {
        case HELP:
            fputs(usage, stderr);
            return EXIT_SUCCESS;
        case SHIFT_VALUE:
            shifts = atoi(optarg);
            break;
        case START_TAG:
            start_tag = atoi(optarg);
            break;
        case NUM_TAGS:
            num_tags = atoi(optarg);
            break;
        case OCCUPIED:
            occupied_only = true;
            break;
        default:
            return EXIT_FAILURE;
        }
    }

    /*
     * Check for compatibility
     */
    const char* display_name = getenv("WAYLAND_DISPLAY");
    if (display_name == NULL) {
        fputs("ERROR: WAYLAND_DISPLAY is not set.\n", stderr);
        return EXIT_FAILURE;
    }

    wl_display = wl_display_connect(display_name);
    if (wl_display == NULL) {
        fputs("ERROR: Can not connect to wayland display.\n", stderr);
        return EXIT_FAILURE;
    }

    /*
     *  1. Use wl_registry handler to retrieve
     *     a. wl_output
     *     b. wl_seat
     *     c. river_status_manager
     *     d. river_controller
     *  2. Use get_river_seat_status to get info about tags
     *     a. focused tags
     *     b. occupied tags
     *     The above are updated to global variables
     *  3. Perform logic to update the tags and set_tagmask()
     */
    struct wl_registry* registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(wl_display);
    river_seat_status = zriver_status_manager_v1_get_river_seat_status(
      river_status_manager, seat);
    zriver_seat_status_v1_add_listener(
      river_seat_status, &river_seat_status_listener, seat);

    wl_display_roundtrip(wl_display);

    /*
     * Rotation logic goes here
     */
    uint32_t new_tagmask = rotate(init_tags, shifts, start_tag, num_tags);
    if (occupied_only &&
        occupied_tags /* if no tags are occupied, do nothing */) {
        new_tagmask = snap_to_occupied(
          new_tagmask, occupied_tags, start_tag, num_tags, shifts);
    }
    set_tagmask(river_controller, new_tagmask);

    /* Cleanup */
    zriver_control_v1_destroy(river_controller);

    return EXIT_SUCCESS;
}
