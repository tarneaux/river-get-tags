#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "river-control-unstable-v1.h"
#include "river-status-unstable-v1.h"

int ret = EXIT_SUCCESS;

struct wl_display* wl_display = NULL;
struct wl_output* wl_output = NULL;
struct wl_seat* seat = NULL;

struct zriver_status_manager_v1* river_status_manager = NULL;
struct zriver_control_v1* river_controller = NULL;

struct zriver_output_status_v1* river_output_status = NULL;

int shifts = 1;
int num_tags = 9;
int start_tag = 0;

unsigned int new_tags = 0;

const char usage[] =
    "Usage: river-shiftview [OPTIONS]\n"
    "   --shifts VALUE     value by which to rotate selected tag.\n"
    "   --num-tags VALUE   number of tags to rotate\n"
    "   --start VALUE      First tag to rotate from.\n"
    "   --help             print this message and exit.\n"
    "\n"
    "\n";

void rotate(unsigned int tagmask, int rotations, int start_tag, int num_tags)
{
    rotations %= num_tags;

    const unsigned int mask = ~(~(0U) << num_tags) << start_tag;
    unsigned int to_rotate = (tagmask & mask) >> start_tag;

    if (rotations < 1) {
	rotations = -rotations;
	new_tags =
	    (mask & ((to_rotate >> rotations) |
	             (to_rotate << (num_tags - rotations)) << start_tag)) +
	    (~mask & tagmask);
    } else {
	new_tags =
	    (mask & ((to_rotate << rotations) |
	             (to_rotate >> (num_tags - rotations)) << start_tag)) +
	    (~mask & tagmask);
    }
}

static void update_tagmask(void* data,
                           struct zriver_output_status_v1* output_status_v1,
                           unsigned int tagmask)
{
    rotate(tagmask, shifts, start_tag, num_tags);
}

static void river_status_handle_view_tags(
    void* data,
    struct zriver_output_status_v1* river_status,
    struct wl_array* tags)
{
}

static void river_status_handle_urgent_tags(
    void* data,
    struct zriver_output_status_v1* river_status,
    uint32_t tags)
{
}

static const struct zriver_output_status_v1_listener output_status_listener = {
    .focused_tags = update_tagmask,
    .view_tags = river_status_handle_view_tags,
    .urgent_tags = river_status_handle_urgent_tags,
};

static void global_registry_handler(void* data,
                                    struct wl_registry* registry,
                                    uint32_t id,
                                    const char* interface,
                                    uint32_t version)
{
    if (strcmp(interface, "wl_output") == 0) {
	wl_output = wl_registry_bind(registry, id, &wl_output_interface, 1);
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

static void global_registry_remover(void* data,
                                    struct wl_registry* registry,
                                    uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

int main(int argc, char* argv[])
{
    enum { HELP, SHIFT_VALUE, START_TAG, NUM_TAGS };
    static struct option opts[] = {
        {"help", no_argument, NULL, HELP},
        {"shifts", required_argument, NULL, SHIFT_VALUE},
        {"num-tags", required_argument, NULL, NUM_TAGS},
        {"start", required_argument, NULL, START_TAG},
        {NULL, 0, NULL, 0},
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
	default:
	    return EXIT_FAILURE;
	}
    }

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

    struct wl_registry* registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(wl_display);

    river_output_status = zriver_status_manager_v1_get_river_output_status(
        river_status_manager, wl_output);

    zriver_output_status_v1_add_listener(river_output_status,
                                         &output_status_listener, NULL);

    /* Wait till new_tags is populated */
    wl_display_roundtrip(wl_display);

    /* Set the new focused tags */
    zriver_control_v1_add_argument(river_controller, "set-focused-tags");

    char* tags_str = malloc((size_t)snprintf(NULL, 0, "%d", new_tags));
    sprintf(tags_str, "%d", new_tags);

    zriver_control_v1_add_argument(river_controller, tags_str);

    zriver_control_v1_run_command(river_controller, seat);

    zriver_control_v1_destroy(river_controller);
    wl_display_roundtrip(wl_display);

    return ret;
}
