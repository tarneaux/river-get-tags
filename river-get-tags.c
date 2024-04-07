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
    printf(
      "\nriver-get-tags: retrieve currently focused and occupied tags in the River compositor on Wayland.\n\n\
Usage:\triver-get-tags (Produce exact same output as river-get-tags -fonF%%6b)\n\
\triver-get-tags [options]\n\n\
Options:\n\
  -h\t\tShow this help message and exit\n\
  -f\t\tPrint focused tags value\n\
  -o\t\tPrint occupied tags value\n\
  -n\t\tPrint tags names in front of value\n\
  -F <format>\tSpecify print format\n\
\t\t- %%[1-32]b for bitfield, or\n\
\t\t- C printf format with with (u, o, x, X) specifiers for printing tags value as unsigned integer,\n\
\t\t   octal, lowercase hexadecimal or UPPERCASE HEXADECIMAL\n\n\
Examples:\n\
  river-get-tags -f -F %%9b\tWill print the focused tags bitfield from bit 0 (tag 1) to\n\
                           \tbit 8 (tag 9), e.g. '110110000'\n\
  river-get-tags -f -F %%u\tWill print the focused tags bitfield value as an unsigned integer, e.g. '27'\n\
  river-get-tags -f -F %%05u\tWill print the focused tags bitfield value as an unsigned integer of at\n\
                            \tleast 5 digits, padding with zeros, e.g. '00027'\n\
  river-get-tags -on -F %%o\tWill print the occupied tags bitfield name & value as an unsigned octal,\n\
                           \te.g. 'occupied: 223'\n\
  river-get-tags -fon -F %%#08x\tWill print the focused tags bitfield name & value then, on new line,\n\
                               \tthe occupied tags bitfield name & value as an unsigned hexadecimal of\n\
                               \tat least 8 digits, in lower case, padding with zeros and with '0x' prefix,\n\
                               \te.g. 'focused: 0x00001b\n\
                               \t      occupied: 0x000093'\n\
\n");
}

void
print_invalid_format(char* cmd, char* s_format)
{
    printf("%s: invalid format for option -F -- `%s`\n", cmd, s_format);
}

enum format_type { BITFIELD, STANDARD };

struct print_options {
    bool focused;
    bool occupied;
    bool names;
    enum format_type ftype;
    int bflength;
    char* format;
};

void
parse_args(int argc, char* argv[], struct print_options* po)
{
    int option;
    while ((option = getopt(argc, argv, "fonF:h")) != -1) {
        switch (option) {
        case 'f':
            po->focused = true;
            break;
        case 'o':
            po->occupied = true;
            break;
        case 'n':
            po->names = true;
            break;
        case 'F': // parse format
            char first = optarg[0];
            char last = optarg[strlen(optarg) - 1];
            char* spec = strchr("buoxX", last);
            if (first != '%' || spec == NULL) {
                print_invalid_format(argv[0], optarg);
                print_usage();
                exit(2);
            }
            if (last == 'b') {
                po->ftype = BITFIELD;
                char s_bflen[strlen(optarg) - 2 + 1];
                strncpy(s_bflen, optarg + 1, strlen(optarg) - 2);
                po->bflength = atoi(s_bflen);
                if (po->bflength < 1 || po->bflength > 32) {
                    print_invalid_format(argv[0], optarg);
                    print_usage();
                    exit(2);
                }
            } else {
                po->ftype = STANDARD;
                po->format = optarg;
            }
            break;
        case 'h':
            print_usage();
            exit(0);
            break;
        case '?':
        default:
            print_usage();
            exit(2);
        }
    }
}

void
print_tags(uint32_t tags, char* name, struct print_options* po)
{
    if (po->names) printf("%s: ", name);
    if (po->ftype == BITFIELD) {
        printbits(tags, po->bflength);
    } else { // STANDARD
        printf(po->format, tags);
    }
    printf("\n");
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

    struct print_options po = {
        true, true, true, BITFIELD, 6, ""
    }; // default output

    if (argc > 1) { // parse arguments
        po.focused = false;
        po.occupied = false;
        po.names = false;
        parse_args(argc, argv, &po);
    }
    if (po.focused) print_tags(focused_tags, "focused", &po);
    if (po.occupied) print_tags(occupied_tags, "occupied", &po);

    /* Cleanup */
    zriver_control_v1_destroy(river_controller);

    return EXIT_SUCCESS;
}
