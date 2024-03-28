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
uint32_t focused_tags = 0; // Added for focused tags

static void
noop()
{
}

uint32_t
get_update_mask(int start_tag, int num_tags)
{
    return ~(num_tags < (int)sizeof(init_tags) * CHAR_BIT ? (~(0U) << num_tags)
                                                          : 0U)
           << start_tag;
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
    focused_tags = tagmask; // Modified to set focused_tags
    init_tags = tagmask;
}

static const struct zriver_output_status_v1_listener output_status_listener = {
    .focused_tags = set_init_tagmask, // Modified to set focused_tags
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

void
printbits(unsigned int v, unsigned int n)
{
    int i; // for C89 compatability
    for (i = 0; i < n; i++)
        putchar('0' + ((v >> i) & 1));
}

void
print_usage()
{
    printf("river-get-tags [TAG FORMAT] [-b BITFIELD_LENGTH] with:\n\
  TAG:\n\
  -f for focused tags\n\
  -o for occupied tags\n\
  FORMAT:\n\
  b for bitfield\n\
  d for decimal\n\
  x for hexadecimal\n");
}

int
main(int argc, char* argv[])
{
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
     *  3. Output the currently focused tags
     */
    struct wl_registry* registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(wl_display);
    river_seat_status = zriver_status_manager_v1_get_river_seat_status(
      river_status_manager, seat);
    zriver_seat_status_v1_add_listener(
      river_seat_status, &river_seat_status_listener, seat);

    wl_display_roundtrip(wl_display);

    if (argc < 2) { // default output
        // Output the focused tags
        int length = 5;
        printf("focused: ");
        printbits(focused_tags, length);
        printf("\n");

        // Output the occupied tags
        printf("occupied: ");
        printbits(occupied_tags, length);
        printf("\n");
    } else { // parsing options
        int option;
        int fflag = 0; // request focused tags printing
        int oflag = 0; // request occupied tags printing
        int bflag = 0; // specify length of printed bitfield
        char fformat;  // format for focused tags:  [d]ecimal | [b]itfield
        char oformat;  // format for occupied tags: [d]ecimal | [b]itfield
        unsigned int blength = 5; // length of bitfields - 5 as default value
        const unsigned int MAX_BITFIELD_LENGTH = 32;
        while ((option = getopt(argc, argv, "f:o:b:")) != -1) {
            switch (option) {
            case 'f':
                fflag++;
                fformat = *optarg;
                break;
            case 'o':
                oflag++;
                oformat = *optarg;
                break;
            case 'b':
                bflag++;
                unsigned int iarg = atoi(optarg);
                blength =
                  (iarg < MAX_BITFIELD_LENGTH) ? iarg : MAX_BITFIELD_LENGTH;
                break;
            default:
                print_usage();
            }
        }
        // print as per options
        if (fflag) {
            switch (fformat) {
            case 'b':
                printbits(focused_tags, blength);
                printf("\n");
                break;
            case 'd':
                printf("%d\n", focused_tags);
                break;
            case 'x':
                printf("%x\n", focused_tags);
                break;
            default:
                printf("Unrecognized format: %c\n", fformat);
                print_usage();
            }
        }
        if (oflag) {
            switch (oformat) {
            case 'b':
                printbits(occupied_tags, blength);
                printf("\n");
                break;
            case 'd':
                printf("%d\n", occupied_tags);
                break;
            case 'x':
                printf("%x\n", occupied_tags);
                break;
            default:
                printf("Unrecognized format: %c\n", oformat);
                print_usage();
            }
        }
    }

    /* Cleanup */
    zriver_control_v1_destroy(river_controller);

    return EXIT_SUCCESS;
}
