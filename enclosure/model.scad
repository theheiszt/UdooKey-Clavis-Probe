// ==============================================================================
// UDOO KEY SINGLE ST7735 DISPLAY & 5-BUTTON PORTRAIT ENCLOSURE
// ==============================================================================
// Parametric OpenSCAD model for a 2-part 3D-printable handheld enclosure.
// Custom-engineered for UDOO KEY (130 x 40 x 10.9 mm) Logic Scope & Bus Pirate.
//
// Key Features:
// - Portrait handheld instrument orientation (40mm wide face x 130mm tall)
// - Single 1.44" ST7735 SPI TFT display window with viewing chamfer
// - 5-Button navigation keypad (UP, DOWN, LEFT, RIGHT, SELECT)
// - 6 Raised standoffs for fastening the UDOO KEY PCB to the base with M2 screws
// - Precision USB-C side cutout for programming and power
// - Front probe bus cutout for logic analyzer test leads (GP2..GP9)
// - 3D-printable captive button plungers / caps
// - Fastening options: M3 corner screws or slip-fit friction lid
//
// Compatibility: 100% self-contained Vanilla OpenSCAD (No external libraries required)
// ==============================================================================

$fn = 48;

/* [Output Selection] */
// Output / visualization mode
part_mode = "both"; // [both: Base & Lid Side-by-Side, base: Base Half Only, lid: Lid Half Only, assembled: Assembled Preview, exploded: Exploded Inspection, wiring: Complete Wiring Routing Inspection, test_coupon: Rapid 10-Min Fit Coupon]

// Explode distance in mm (for exploded view)
explode = 30.0; // [10:5:60]

// Show PCB mockup in assembly/exploded preview
show_pcb = true;

// Show ST7735 display mockup in assembly/exploded preview
show_display = true;

// Show tactile switches in assembly/exploded preview
show_buttons = true;

// Show add-on daughterboard PCB mockup in assembly/exploded preview
show_daughterboard = true;

// Show color-coded wiring harness in assembly/wiring preview
show_wiring = true;

/* [Core Board Dimensions] */
// Swapped XY for portrait face layout (X=40, Y=130)
board_len = 40.0;   // PCB width across narrow axis (mm)
board_w   = 130.0;  // PCB length along long axis (mm)
board_h   = 10.9;   // Total component envelope height (mm)
pcb_t     = 1.6;    // Bare PCB substrate thickness (mm)
pico_len  = 51.0;   // Length of narrow Pico blade section (mm)
pico_w    = 21.0;   // Width of narrow Pico blade section (mm)

/* [Raised Standoffs for UDOO KEY PCB] */
// Raised standoff height above bottom floor (mm)
standoff_h  = 5.0;

// Standoff outer diameter (mm)
standoff_od = 6.0;

// Standoff screw pilot hole diameter for M2 screws (mm)
standoff_id = 2.0;

// Conical strengthening fillet radius at standoff base (mm)
standoff_fillet = 1.2;

/* [Enclosure Wall & Floor Thickness] */
// Bottom case outer wall thickness (mm)
base_wall = 2.4; // [1.6:0.2:4.5]

// Top lid outer wall thickness (mm)
lid_wall = 2.4; // [1.6:0.2:4.5]

// Base floor thickness (mm)
floor_t = 2.4; // [1.6:0.2:4.0]

// Lid top faceplate thickness (mm)
lid_t = 2.2; // [1.6:0.2:4.0]

// XY gap around PCB body inside cavity (mm)
clearance = 1.4; // [0.8:0.2:3.0]

// Outer corner radius (mm)
corner_r = 6.0; // [2.0:1.0:10.0]

/* [Lid & Base Overlap Joint Settings] */
// Thickness of the interlocking lip/tongue on the bottom case (mm)
lip_thickness = 1.2; // [0.6:0.1:2.4]

// Height of the interlocking lip extending above the bottom case rim (mm)
lip_height = 4.0; // [1.5:0.5:10.0]

// Depth of the mating recess/pocket inside the top lid (mm)
lid_recess_depth = 4.6; // [2.0:0.5:12.0]

// Horizontal sliding fit gap between lip and lid recess (mm)
lip_clearance_xy = 0.25; // [0.10:0.05:0.50]

// 45-degree lead-in entry chamfer on top of the lip (mm)
lip_lead_chamfer = 0.8; // [0.0:0.2:1.5]

// Corner fastening screw options
use_corner_screws = true;
screw_d      = 3.2;    // M3 screw clearance hole (mm)
screw_head_d = 6.2;    // M3 screw head counterbore diameter (mm)
screw_head_h = 3.2;    // M3 screw head counterbore depth (mm)
screw_boss_d = 8.0;    // Corner screw pillar diameter (mm)
insert_d     = 4.2;    // M3 brass heat-set insert hole diameter (mm)

/* [USB-C Side Cutout] */
// Side wall for USB-C cable slot
usbc_side = "left"; // [left: Left Side Wall (Matches official CAD drawing), right: Right Side Wall]

// Width of cutout along Y (mm)
usb_w            = 13.5;

// Total height of cutout along Z (mm)
usb_h            = 7.0;

// Position along board length from bottom (mm)
usb_y_center     = 60.0;

// Height from bottom floor to bottom of USB-C port (mm)
usb_z_from_floor = 6.6;

/* [Front Logic Analyzer Probe Cutout] */
add_probe_slot = true;
probe_slot_w   = 18.0;  // Width along X (mm)
probe_slot_h   = 7.0;   // Height along Z (mm)

/* [Single ST7735 Display Module] */
// Outer PCB dimensions of 1.44" ST7735 module (edit if your module differs)
tft_pcb_w = 34.0;
tft_pcb_h = 44.0;

// Front visible window cutout
tft_window_w = 27.0;
tft_window_h = 27.0;

// 45-degree viewing bezel chamfer on top of lid
tft_chamfer = 1.2;

// Lid underside recess around display body
tft_recess_w = 31.0;
tft_recess_h = 33.0;
tft_recess_d = 1.6;

// Center position of display on front faceplate (from bottom edge)
tft_center_y = 86.0;

/* [Five-Button Navigation Keypad (Direct 6x6mm Tactile Switches - No Plungers)] */
// Button layout style
button_layout = "cross_and_select"; // [cross_and_select: D-Pad Cross with SELECT below, diamond_center_select: Diamond with SELECT in Center, row5: Ergonomic 5-in-a-Row]

// 6x6mm Tactile Switch Dimensions (Pocket on lid underside)
switch_body_w    = 6.2;  // Pocket width with 0.2mm clearance (mm)
switch_body_l    = 6.2;  // Pocket length with 0.2mm clearance (mm)
switch_pocket_h  = 3.6;  // Retention cradle depth inside lid (mm)
switch_pocket_t  = 1.1;  // Retention cradle wall thickness (mm)

// Actuator stem aperture through faceplate
switch_stem_d    = 3.5;  // Switch round actuator stem diameter (mm)
button_hole_d    = 3.9;  // Thru-hole diameter for switch stem (mm)

// Ergonomic concave finger dish on top face
dish_d           = 9.6;  // Diameter of concave finger recess (mm)
dish_depth       = 1.1;  // Depth of finger dish into faceplate (mm)

// Center-to-center button spacing (mm)
button_spacing = 10.0;

// Center Y position of button keypad cluster on front faceplate (mm)
button_block_center_y = 38.0;

/* [Right-Side Daughterboard Extension & Headroom] */
// Extra width to the right (+X) for daughterboard/shield on RP2040 pinout (mm)
extra_right = 25.0; // [0.0:1.0:60.0]

// Extra vertical (Z-axis) headroom above PCB for daughterboard & stacking headers (mm)
extra_z     = 4.0;  // [0.0:1.0:15.0]

// UI Alignment across X axis ("board" = centered over UDOO KEY; "enclosure" = centered on outer face)
ui_alignment = "board"; // [board: Centered over UDOO KEY Board, enclosure: Centered on Enclosure Face]

/* [Manufacturing & Tolerances] */
elephants_foot = 0.6; // Bed contact chamfer (mm)
eps  = 0.02;
eps2 = 0.04;

// ==============================================================================
// DERIVED DIMENSIONS & MECHANICAL ASSERTIONS
// ==============================================================================

// Aliases for compatibility
wall = base_wall;
lid_overlap = lip_height;
fit_gap = lip_clearance_xy;

// Base horizontal shelf width (outer ledge on base rim where lid skirt sits)
shelf_w = base_wall - lip_thickness;

// Recess outer boundary inside top lid (leaves a skirt wall of thickness shelf_w - lip_clearance_xy)
recess_offset = shelf_w - lip_clearance_xy;

inner_len = board_len + clearance * 2 + extra_right; // ~67.8 mm (X axis)
inner_w   = board_w   + clearance * 2;               // ~132.8 mm (Y axis)
inner_h   = standoff_h + pcb_t + board_h + 1.0 + extra_z; // ~22.5 mm (Z axis)

outer_len = inner_len + base_wall * 2;               // ~72.6 mm
outer_w   = inner_w   + base_wall * 2;               // ~137.6 mm
base_h    = standoff_h + pcb_t + 4.5;                // ~11.1 mm (parting line at USB-C mid-height)
lid_h     = (inner_h + floor_t + lid_t) - base_h;    // ~16.2 mm

face_center_x = outer_len / 2;
face_center_y = outer_w / 2;

// PCB origin inside internal cavity (anchored on the left wall where USB-C is located)
pcb_x0 = base_wall + clearance;
pcb_y0 = base_wall + clearance;

// Center X for UI (Display & 5 Buttons) and Probe Slot
board_center_x = pcb_x0 + board_len / 2;
ui_center_x    = (ui_alignment == "board") ? board_center_x : face_center_x;

// Standoff coordinates (X, Y) relative to enclosure outer origin
// Derived from measured UDOO KEY hardware specs:
// Pico blade section at Y=0..51mm, wide section at Y=51..130mm
// Pico mounting holes: X = 20.0 +/- 5.7mm (14.3 and 25.7), Y = 2.0 and 49.0mm
// Wide section mounting holes: X = 20.0 +/- 17.0mm (3.0 and 37.0), Y = 120.0mm
pcb_standoffs = [
    [pcb_x0 + 14.3, pcb_y0 + 2.0],   // Pico Bottom Left
    [pcb_x0 + 25.7, pcb_y0 + 2.0],   // Pico Bottom Right
    [pcb_x0 + 14.3, pcb_y0 + 49.0],  // Pico Upper Left
    [pcb_x0 + 25.7, pcb_y0 + 49.0],  // Pico Upper Right
    [pcb_x0 + 3.0,  pcb_y0 + 120.0], // Wide Top Left
    [pcb_x0 + 37.0, pcb_y0 + 120.0]  // Wide Top Right
];

// Corner screw locations (X, Y)
corner_screws = [
    [base_wall + screw_boss_d/2 - 0.5, base_wall + screw_boss_d/2 - 0.5],
    [outer_len - (base_wall + screw_boss_d/2 - 0.5), base_wall + screw_boss_d/2 - 0.5],
    [base_wall + screw_boss_d/2 - 0.5, outer_w - (base_wall + screw_boss_d/2 - 0.5)],
    [outer_len - (base_wall + screw_boss_d/2 - 0.5), outer_w - (base_wall + screw_boss_d/2 - 0.5)]
];

// 5-Button Coordinates Calculation
function get_button_coords() =
    (button_layout == "cross_and_select") ? [
        [ui_center_x, button_block_center_y + button_spacing],        // UP
        [ui_center_x, button_block_center_y - button_spacing],        // DOWN
        [ui_center_x - button_spacing, button_block_center_y],        // LEFT
        [ui_center_x + button_spacing, button_block_center_y],        // RIGHT
        [ui_center_x, button_block_center_y - 2.2 * button_spacing]   // SELECT
    ] :
    (button_layout == "diamond_center_select") ? [
        [ui_center_x, button_block_center_y + button_spacing],        // UP
        [ui_center_x, button_block_center_y - button_spacing],        // DOWN
        [ui_center_x - button_spacing, button_block_center_y],        // LEFT
        [ui_center_x + button_spacing, button_block_center_y],        // RIGHT
        [ui_center_x, button_block_center_y]                          // SELECT (Center)
    ] : [
        // row5
        [ui_center_x - 2 * button_spacing, button_block_center_y],
        [ui_center_x - 1 * button_spacing, button_block_center_y],
        [ui_center_x,                      button_block_center_y],
        [ui_center_x + 1 * button_spacing, button_block_center_y],
        [ui_center_x + 2 * button_spacing, button_block_center_y]
    ];

btn_coords = get_button_coords();

// Boundary assertions
assert(base_wall >= 1.6, "Base wall thickness must be at least 1.6mm!");
assert(lid_wall >= 1.6, "Lid wall thickness must be at least 1.6mm!");
assert(floor_t >= 1.6, "Floor thickness must be at least 1.6mm!");
assert(lip_thickness < base_wall, "Lip thickness must be strictly less than base wall thickness!");
assert(shelf_w > lip_clearance_xy, "Base shelf (base_wall - lip_thickness) must be wider than lip_clearance_xy!");
assert(lid_recess_depth >= lip_height, "Lid recess depth must be >= lip height!");
assert(button_hole_d > switch_stem_d, "Button hole must be larger than switch actuator stem!");
assert(dish_depth < lid_t, "Finger dish depth must be strictly less than lid faceplate thickness!");


// ==============================================================================
// LOW-LEVEL PRIMITIVES & HELPERS
// ==============================================================================

module rounded_box(size = [10,10,10], r = 2, chamfer_bot = 0) {
    l = size[0];
    w = size[1];
    h = size[2];
    hull() {
        for (x = [r, l - r]) {
            for (y = [r, w - r]) {
                translate([x, y, 0]) {
                    if (chamfer_bot > 0) {
                        cylinder(r1 = r - chamfer_bot, r2 = r, h = chamfer_bot);
                        translate([0, 0, chamfer_bot])
                            cylinder(r = r, h = h - chamfer_bot);
                    } else {
                        cylinder(r = r, h = h);
                    }
                }
            }
        }
    }
}

module centered_cut(xc, yc, z0, sx, sy, sz) {
    translate([xc - sx/2, yc - sy/2, z0]) cube([sx, sy, sz]);
}

module reinforced_standoff(h, od, id, fillet = 1.2) {
    difference() {
        union() {
            cylinder(d = od, h = h);
            // Conical stress-relief root fillet
            cylinder(d1 = od + 2*fillet, d2 = od, h = fillet);
        }
        translate([0, 0, -eps])
            cylinder(d = id, h = h + 2*eps);
    }
}

module screw_counterbore(h_total, d_shaft, d_head, h_head) {
    translate([0, 0, -eps]) {
        cylinder(d = d_shaft, h = h_total + 2*eps);
        cylinder(d = d_head,  h = h_head + eps);
        translate([0, 0, h_head])
            cylinder(d1 = d_head, d2 = d_shaft, h = (d_head - d_shaft) / 2);
    }
}

// ==============================================================================
// MECHANICAL HARDWARE MOCKUPS
// ==============================================================================

module udoo_key_pcb_mockup() {
    color([0.15, 0.15, 0.18, 0.88]) {
        difference() {
            union() {
                // Pico Blade Section (Y=0..51mm, X centered 21mm wide)
                translate([(board_len - pico_w)/2, 0, 0])
                    cube([pico_w, pico_len, pcb_t]);
                // Wide ESP32 Section (Y=51..130mm, X=40mm wide)
                translate([0, pico_len, 0])
                    cube([board_len, board_w - pico_len, pcb_t]);
            }
            // 6 Mounting holes
            for (st = pcb_standoffs) {
                translate([st[0] - pcb_x0, st[1] - pcb_y0, -eps])
                    cylinder(d = 2.1, h = pcb_t + 2*eps);
            }
        }
    }

    // RP2040 IC
    color([0.1, 0.1, 0.1])
        translate([(board_len - 7)/2, 26, pcb_t]) cube([7, 7, 1.0]);

    // ESP32-WROVER-E Shield
    color([0.78, 0.78, 0.82])
        translate([(board_len - 18)/2, 82, pcb_t]) cube([18, 25.5, 3.2]);

    // USB-C Receptacle (on left or right edge according to usbc_side)
    usbc_mockup_x = (usbc_side == "left") ? 0 : board_len - 8.5;
    color([0.75, 0.75, 0.75])
        translate([usbc_mockup_x, usb_y_center - usb_w/2 + 2.0, pcb_t])
            cube([8.5, 9.0, 3.2]);

    // Pin Headers
    color([0.2, 0.2, 0.2]) {
        translate([(board_len - pico_w)/2 - 1.2, 4, pcb_t]) cube([2.5, 50.8, 8.5]);
        translate([(board_len + pico_w)/2 - 1.3, 4, pcb_t]) cube([2.5, 50.8, 8.5]);
    }
}

module st7735_display_mockup() {
    // Red PCB carrier
    color([0.72, 0.12, 0.12])
        cube([tft_pcb_w, tft_pcb_h, 1.6]);
    // Glass Bezel
    color([0.08, 0.08, 0.08])
        translate([(tft_pcb_w - 30)/2, (tft_pcb_h - 30)/2, 1.6])
            cube([30, 30, 1.8]);
    // Active TFT screen
    color([0.1, 0.35, 0.65])
        translate([(tft_pcb_w - tft_window_w)/2, (tft_pcb_h - tft_window_h)/2, 3.4])
            cube([tft_window_w, tft_window_h, 0.1]);
}

module tactile_switch_6x6_mockup() {
    // Standard 6x6x5mm tactile pushbutton
    // Black plastic base (6.0 x 6.0 x 3.5mm)
    color([0.15, 0.15, 0.15])
        translate([-3.0, -3.0, 0])
            cube([6.0, 6.0, 3.5]);

    // Silver metal bracket / top cover with 4 rivet dots
    color([0.82, 0.82, 0.84]) {
        translate([-3.0, -3.0, 3.5])
            cube([6.0, 6.0, 0.4]);
        // 4 corner rivets
        for (rx = [-2.3, 2.3]) {
            for (ry = [-2.3, 2.3]) {
                translate([rx, ry, 3.9])
                    cylinder(d = 0.8, h = 0.15);
            }
        }
    }

    // Black round actuator stem (3.5mm dia x 1.1mm tall)
    color([0.08, 0.08, 0.08])
        translate([0, 0, 3.9])
            cylinder(d = switch_stem_d, h = 1.1);

    // 4 Solder terminals / pins extending downwards
    color([0.85, 0.85, 0.85]) {
        for (px = [-3.0, 3.0]) {
            for (py = [-2.25, 2.25]) {
                translate([px > 0 ? px - 0.2 : px, py - 0.35, -3.5])
                    cube([0.2, 0.7, 3.5]);
            }
        }
    }
}

module switch_retention_cradle(bx, by) {
    c_w = switch_body_w + 2 * switch_pocket_t;
    c_l = switch_body_l + 2 * switch_pocket_t;
    c_z = lid_h - lid_t - switch_pocket_h;

    difference() {
        // Outer cradle box extending down from lid ceiling
        translate([bx - c_w/2, by - c_l/2, c_z])
            cube([c_w, c_l, switch_pocket_h]);

        // Inner rectangular pocket for 6x6 tactile switch body
        translate([bx - switch_body_w/2, by - switch_body_l/2, c_z - eps])
            cube([switch_body_w, switch_body_l, switch_pocket_h + eps]);

        // 45-degree lead-in entry chamfer at bottom opening for smooth press-fit
        translate([bx - (switch_body_w + 0.8)/2, by - (switch_body_l + 0.8)/2, c_z - eps])
            hull() {
                cube([switch_body_w + 0.8, switch_body_l + 0.8, 0.6]);
                translate([0.4, 0.4, 0.6])
                    cube([switch_body_w, switch_body_l, eps]);
            }

        // Wire & pin exit relief channels (allows soldered wires to route freely)
        // Cuts across X (switch pin side)
        translate([bx - c_w/2 - eps, by - 2.0, c_z - eps])
            cube([c_w + 2*eps, 4.0, switch_pocket_h - 0.8]);

        // Cuts across Y (routing channels)
        translate([bx - 2.0, by - c_l/2 - eps, c_z - eps])
            cube([4.0, c_l + 2*eps, switch_pocket_h - 0.8]);
    }
}

module daughterboard_mockup() {
    // Add-on PCB mounted on RP2040 headers (spanning across RP2040 pinout and extending into the right-hand expansion)
    // Plugs into P1 and P2 pin headers at Z = floor_t + standoff_h + pcb_t + 8.5
    db_x0 = pcb_x0 + (board_len - pico_w)/2 - 1.5;
    db_w  = pico_w + extra_right + 3.0;
    db_l  = pico_len - 2.0;
    db_z  = floor_t + standoff_h + pcb_t + 8.5;

    color([0.18, 0.58, 0.36, 0.88]) { // Translucent green prototyping PCB
        translate([db_x0, pcb_y0 + 3.0, db_z]) {
            difference() {
                cube([db_w, db_l, 1.6]);
                // Mounting hole / pin indicators
                for (hx = [3.5, db_w - 3.5]) {
                    for (hy = [3.5, db_l - 3.5]) {
                        translate([hx, hy, -eps])
                            cylinder(d = 2.0, h = 1.6 + 2*eps);
                    }
                }
            }
        }
    }
}

// ==============================================================================
// 3D COLOR-CODED WIRING HARNESS VISUALIZATION
// ==============================================================================

module wire_segment(p1, p2, d = 0.8) {
    hull() {
        translate(p1) sphere(d = d, $fn = 12);
        translate(p2) sphere(d = d, $fn = 12);
    }
}

module wire_path(pts, d = 0.8, col = [0.8, 0.2, 0.2]) {
    color(col) {
        for (i = [0:len(pts)-2]) {
            wire_segment(pts[i], pts[i+1], d);
        }
    }
}

module wiring_harness() {
    // RP2040 header pin locations
    hdr_l_x = pcb_x0 + (board_len - pico_w)/2 - 1.2;  // Left header X
    hdr_r_x = pcb_x0 + (board_len + pico_w)/2 - 1.3;  // Right header X
    hdr_z   = floor_t + standoff_h + pcb_t + 8.5;     // Top of header pins

    // Pin Y coordinates (Pin 1 & 40 near Y=pcb_y0+4, Pin 20 & 21 near Y=pcb_y0+52.2)
    p_gp11 = [hdr_l_x, pcb_y0 + 39.5, hdr_z]; // Pin 15 (RST)
    p_gp12 = [hdr_l_x, pcb_y0 + 42.0, hdr_z]; // Pin 16 (DC)
    p_gp13 = [hdr_l_x, pcb_y0 + 44.6, hdr_z]; // Pin 17 (CS)
    p_gp15 = [hdr_l_x, pcb_y0 + 52.2, hdr_z]; // Pin 20 (UP)

    p_gp16 = [hdr_r_x, pcb_y0 + 52.2, hdr_z]; // Pin 21 (DOWN)
    p_gp17 = [hdr_r_x, pcb_y0 + 49.7, hdr_z]; // Pin 22 (LEFT)
    p_gnd_btn = [hdr_r_x, pcb_y0 + 47.1, hdr_z]; // Pin 23 (GND)
    p_gp18 = [hdr_r_x, pcb_y0 + 44.6, hdr_z]; // Pin 24 (SCK)
    p_gp19 = [hdr_r_x, pcb_y0 + 42.0, hdr_z]; // Pin 25 (MOSI)
    p_gp21 = [hdr_r_x, pcb_y0 + 37.0, hdr_z]; // Pin 27 (RIGHT)
    p_gp22 = [hdr_r_x, pcb_y0 + 31.9, hdr_z]; // Pin 29 (SELECT)
    p_3v3  = [hdr_r_x, pcb_y0 + 14.1, hdr_z]; // Pin 36 (3V3)
    p_gnd  = [hdr_r_x, pcb_y0 + 9.0,  hdr_z]; // Pin 38 (GND)

    // ST7735 Display pin header (at bottom of display carrier PCB)
    tft_pin_y = tft_center_y - tft_pcb_h/2;
    tft_z     = base_h + lid_h - lid_t - 3.2;

    // Display SPI Wires (Color-coded)
    wire_path([[ui_center_x - 10, tft_pin_y, tft_z], [ui_center_x - 10, tft_pin_y - 6, hdr_z + 4], p_3v3], 0.7, [0.9, 0.2, 0.2]); // VCC (Red)
    wire_path([[ui_center_x - 7,  tft_pin_y, tft_z], [ui_center_x - 7,  tft_pin_y - 6, hdr_z + 4], p_gnd], 0.7, [0.1, 0.1, 0.1]); // GND (Black)
    wire_path([[ui_center_x - 4,  tft_pin_y, tft_z], [ui_center_x - 4,  tft_pin_y - 8, hdr_z + 3], p_gp13], 0.7, [0.9, 0.5, 0.1]); // CS (Orange)
    wire_path([[ui_center_x - 1,  tft_pin_y, tft_z], [ui_center_x - 1,  tft_pin_y - 8, hdr_z + 3], p_gp11], 0.7, [0.9, 0.9, 0.9]); // RST (White)
    wire_path([[ui_center_x + 2,  tft_pin_y, tft_z], [ui_center_x + 2,  tft_pin_y - 8, hdr_z + 3], p_gp12], 0.7, [0.9, 0.9, 0.2]); // DC (Yellow)
    wire_path([[ui_center_x + 5,  tft_pin_y, tft_z], [ui_center_x + 5,  tft_pin_y - 8, hdr_z + 3], p_gp19], 0.7, [0.2, 0.8, 0.2]); // MOSI (Green)
    wire_path([[ui_center_x + 8,  tft_pin_y, tft_z], [ui_center_x + 8,  tft_pin_y - 8, hdr_z + 3], p_gp18], 0.7, [0.2, 0.4, 0.9]); // SCK (Blue)

    // Button Wires (from switch terminals on lid ceiling down to GPIO pins)
    btn_z = base_h + lid_h - lid_t - switch_pocket_h;

    // UP Button -> GP15 (Pin 20)
    b_up = btn_coords[0];
    wire_path([[b_up[0] - 2, b_up[1], btn_z], [b_up[0] - 8, b_up[1] + 2, hdr_z + 2], p_gp15], 0.7, [0.9, 0.3, 0.3]);

    // DOWN Button -> GP16 (Pin 21)
    b_dn = btn_coords[1];
    wire_path([[b_dn[0] + 2, b_dn[1], btn_z], [b_dn[0] + 6, b_dn[1] + 4, hdr_z + 2], p_gp16], 0.7, [0.95, 0.6, 0.2]);

    // LEFT Button -> GP17 (Pin 22)
    b_lf = btn_coords[2];
    wire_path([[b_lf[0] + 2, b_lf[1], btn_z], [b_lf[0] + 8, b_lf[1] + 4, hdr_z + 2], p_gp17], 0.7, [0.9, 0.85, 0.2]);

    // RIGHT Button -> GP21 (Pin 27)
    b_rt = btn_coords[3];
    wire_path([[b_rt[0] + 2, b_rt[1], btn_z], [b_rt[0] + 4, b_rt[1] - 2, hdr_z + 2], p_gp21], 0.7, [0.2, 0.5, 0.9]);

    // SELECT Button -> GP22 (Pin 29)
    b_sel = btn_coords[4];
    wire_path([[b_sel[0] + 2, b_sel[1], btn_z], [b_sel[0] + 4, b_sel[1] + 4, hdr_z + 2], p_gp22], 0.7, [0.7, 0.3, 0.85]);

    // Button Ground Daisy Chain -> GND (Pin 23)
    wire_path([[b_sel[0] - 2, b_sel[1], btn_z], [b_dn[0] - 2, b_dn[1], btn_z], [b_lf[0] - 2, b_lf[1], btn_z], [b_up[0] + 2, b_up[1], btn_z], [b_rt[0] - 2, b_rt[1], btn_z], p_gnd_btn], 0.6, [0.15, 0.15, 0.15]);
}

module wiring_preview() {
    // Base half
    base();

    // UDOO KEY PCB seated on standoffs
    translate([pcb_x0, pcb_y0, floor_t + standoff_h])
        udoo_key_pcb_mockup();

    // Add-on Daughterboard PCB
    if (show_daughterboard && extra_right > 0) {
        daughterboard_mockup();
    }

    // Display module
    translate([ui_center_x - tft_pcb_w/2, tft_center_y - tft_pcb_h/2, base_h + lid_h - lid_t - 3.2])
        st7735_display_mockup();

    // 5 Tactile Switches
    for (p = btn_coords) {
        translate([p[0], p[1], base_h + lid_h - lid_t - 3.9])
            tactile_switch_6x6_mockup();
    }

    // Full 3D Color-Coded Wiring Harness
    wiring_harness();

    // Semi-transparent ghosted lid outline
    color([0.22, 0.28, 0.38, 0.22]) {
        translate([0, 0, base_h])
            lid();
    }
}

// ==============================================================================
// BASE MODULE (BOTTOM HALF WITH 6 RAISED STANDOFFS)
// ==============================================================================

module base() {
    color([0.20, 0.22, 0.26]) {
        difference() {
            union() {
                // 1. Hollow bottom case shell with interlocking lip
                difference() {
                    union() {
                        // Main outer bottom shell with elephant's foot bed chamfer
                        rounded_box([outer_len, outer_w, base_h], corner_r, elephants_foot);

                        // Perimeter Interlocking Step Lip (Male tongue on base)
                        translate([shelf_w, shelf_w, base_h]) {
                            difference() {
                                // Outer lip shape with lead-in chamfer on top
                                if (lip_lead_chamfer > 0 && lip_height > lip_lead_chamfer) {
                                    hull() {
                                        rounded_box([outer_len - 2*shelf_w, outer_w - 2*shelf_w, lip_height - lip_lead_chamfer], max(1, corner_r - shelf_w));
                                        translate([lip_lead_chamfer, lip_lead_chamfer, lip_height - eps])
                                            rounded_box([outer_len - 2*(shelf_w + lip_lead_chamfer), outer_w - 2*(shelf_w + lip_lead_chamfer), eps], max(0.5, corner_r - shelf_w - lip_lead_chamfer));
                                    }
                                } else {
                                    rounded_box([outer_len - 2*shelf_w, outer_w - 2*shelf_w, lip_height], max(1, corner_r - shelf_w));
                                }

                                // Hollow interior flush with inner cavity
                                translate([lip_thickness, lip_thickness, -eps])
                                    rounded_box([inner_len, inner_w, lip_height + 2*eps], max(1, corner_r - base_wall));
                            }
                        }
                    }

                    // Hollow Out Internal Cavity
                    translate([base_wall, base_wall, floor_t])
                        rounded_box([inner_len, inner_w, base_h - floor_t + lip_height + eps2], max(1, corner_r - base_wall));

                    // USB-C Cutout on Side Wall (left or right according to usbc_side)
                    usbc_x_pos = (usbc_side == "left") ? -eps : outer_len - base_wall - eps;
                    translate([
                        usbc_x_pos,
                        pcb_y0 + usb_y_center - usb_w/2,
                        floor_t + usb_z_from_floor
                    ]) cube([base_wall + 2*eps, usb_w, base_h]);

                    // Front Probe Bus Cutout on the Bottom End (Y = 0)
                    if (add_probe_slot) {
                        translate([
                            board_center_x - probe_slot_w/2,
                            -eps,
                            floor_t + standoff_h + pcb_t
                        ]) cube([probe_slot_w, base_wall + 2*eps, base_h]);
                    }
                }

                // 2. 6x Raised Standoffs for UDOO KEY PCB (Solid on floor inside cavity)
                for (st = pcb_standoffs) {
                    translate([st[0], st[1], floor_t])
                        reinforced_standoff(standoff_h, standoff_od, standoff_id, standoff_fillet);
                }

                // 3. 4x Corner Screw Pillars
                if (use_corner_screws) {
                    for (cs = corner_screws) {
                        translate([cs[0], cs[1], floor_t])
                            cylinder(d = screw_boss_d, h = base_h - floor_t);
                    }
                }
            }

            // 4. Subtract Corner Screw Counterbores from Bottom
            if (use_corner_screws) {
                for (cs = corner_screws) {
                    translate([cs[0], cs[1], 0])
                        screw_counterbore(base_h + eps, screw_d, screw_head_d, screw_head_h);
                }
            }
        }
    }
}

// ==============================================================================
// LID MODULE (TOP HALF WITH 1 DISPLAY & 5 BUTTONS)
// ==============================================================================

module lid() {
    color([0.16, 0.18, 0.22]) {
        difference() {
            union() {
                // 1. Hollow top case shell with windows, holes, and recess
                difference() {
                    // Main top outer shell
                    rounded_box([outer_len, outer_w, lid_h], corner_r);

                    // Hollow Out Internal Cavity
                    translate([lid_wall, lid_wall, -eps])
                        rounded_box([inner_len, inner_w, lid_h - lid_t + eps], max(1, corner_r - lid_wall));

                    // Female Recess / Pocket mating over Base Lip
                    // Extends from Z = -eps up to Z = lid_recess_depth
                    translate([recess_offset, recess_offset, -eps])
                        rounded_box([
                            outer_len - 2*recess_offset,
                            outer_w   - 2*recess_offset,
                            lid_recess_depth + eps
                        ], max(1, corner_r - recess_offset));

                    // Single ST7735 Display Viewport Window (Centered at ui_center_x, tft_center_y)
                    translate([ui_center_x - tft_window_w/2, tft_center_y - tft_window_h/2, lid_h - lid_t - eps]) {
                        cube([tft_window_w, tft_window_h, lid_t + 2*eps]);
                        // 45-degree Bezel Viewing Chamfer on top face
                        translate([-tft_chamfer, -tft_chamfer, lid_t - tft_chamfer])
                            hull() {
                                cube([tft_window_w + 2*tft_chamfer, tft_window_h + 2*tft_chamfer, tft_chamfer + eps]);
                            }
                    }

                    // Shallow recess on lid underside for display module glass
                    translate([ui_center_x - tft_recess_w/2, tft_center_y - tft_recess_h/2, lid_h - lid_t - tft_recess_d])
                        cube([tft_recess_w, tft_recess_h, tft_recess_d + eps]);

                    // Display PCB pocket
                    translate([ui_center_x - tft_pcb_w/2, tft_center_y - tft_pcb_h/2, lid_h - lid_t - 3.2 - eps])
                        cube([tft_pcb_w, tft_pcb_h, 3.2 + eps]);

                    // 5-Button Keypad Apertures & Concave Finger Dishes (No Plungers)
                    for (p = btn_coords) {
                        bx = p[0];
                        by = p[1];
                        // Precision thru-hole for switch actuator stem (3.9mm hole for 3.5mm stem)
                        translate([bx, by, lid_h - lid_t - eps])
                            cylinder(d = button_hole_d, h = lid_t + 2*eps);

                        // Ergonomic concave finger dish on front faceplate
                        translate([bx, by, lid_h - dish_depth])
                            cylinder(d1 = button_hole_d + 1.0, d2 = dish_d, h = dish_depth + eps);
                    }

                    // Upper half of USB-C Side Cutout (matching usbc_side)
                    usbc_x_pos_lid = (usbc_side == "left") ? -eps : outer_len - lid_wall - eps;
                    usbc_cut_h_lid = max(2.5, usb_h - (base_h - (floor_t + usb_z_from_floor)));
                    translate([
                        usbc_x_pos_lid,
                        pcb_y0 + usb_y_center - usb_w/2,
                        -eps
                    ]) cube([lid_wall + 2*eps, usb_w, usbc_cut_h_lid + eps]);

                    // Upper half of Front Probe Cutout on Y = 0 wall
                    if (add_probe_slot) {
                        translate([
                            board_center_x - probe_slot_w/2,
                            -eps,
                            -eps
                        ]) cube([probe_slot_w, lid_wall + 2*eps, probe_slot_h / 2]);
                    }
                }

                // 2. Corner Screw Bosses for M3 brass inserts
                if (use_corner_screws) {
                    for (cs = corner_screws) {
                        translate([cs[0], cs[1], 0])
                            cylinder(d = screw_boss_d, h = lid_h - lid_t);
                    }
                }

                // 3. Display Module Retaining Stops / Frame on Ceiling
                translate([ui_center_x - tft_pcb_w/2 - 1.5, tft_center_y - tft_pcb_h/2 - 1.5, lid_h - lid_t - 3.2])
                    difference() {
                        cube([tft_pcb_w + 3.0, tft_pcb_h + 3.0, 3.2]);
                        translate([1.5, 1.5, -eps])
                            cube([tft_pcb_w, tft_pcb_h, 3.2 + 2*eps]);
                    }

                // 4. 5x Tactile Switch Retention Pockets on Lid Underside (Snap/Friction Fit)
                for (p = btn_coords) {
                    switch_retention_cradle(p[0], p[1]);
                }
            }

            // 5. Subtract Corner Screw Insert Holes
            if (use_corner_screws) {
                for (cs = corner_screws) {
                    translate([cs[0], cs[1], -eps])
                        cylinder(d = insert_d, h = 6.0);
                }
            }
        }
    }
}

// ==============================================================================
// RAPID 5-MINUTE DFM CALIBRATION TEST COUPON
// ==============================================================================

module test_coupon() {
    difference() {
        union() {
            rounded_box([52.0, 36.0, 8.0], 3.0, elephants_foot);
            // 1x Standoff boss with conical root fillet
            translate([10.0, 18.0, 8.0])
                reinforced_standoff(standoff_h, standoff_od, standoff_id, standoff_fillet);
            // 1x Sample Base Lip with chamfer
            translate([4.0, 4.0, 8.0]) {
                difference() {
                    cube([14.0, lip_thickness, lip_height]);
                    if (lip_lead_chamfer > 0) {
                        translate([-eps, lip_thickness - lip_lead_chamfer, lip_height - lip_lead_chamfer])
                            rotate([45, 0, 0])
                                cube([14.0 + 2*eps, lip_lead_chamfer * 2, lip_lead_chamfer * 2]);
                    }
                }
            }
        }
        // 1x 6x6mm Tactile Switch test pocket (from underside)
        translate([26.0 - switch_body_w/2, 18.0 - switch_body_l/2, -eps])
            cube([switch_body_w, switch_body_l, 8.0 - lid_t + eps]);
        // Wire & pin exit reliefs for switch
        translate([26.0 - 5.0, 18.0 - 2.0, -eps])
            cube([10.0, 4.0, 8.0 - lid_t]);
        // 1x Thru-hole for switch actuator stem
        translate([26.0, 18.0, 8.0 - lid_t - eps])
            cylinder(d = button_hole_d, h = lid_t + 2*eps);
        // 1x Concave finger dish on top face
        translate([26.0, 18.0, 8.0 - dish_depth])
            cylinder(d1 = button_hole_d + 1.0, d2 = dish_d, h = dish_depth + eps);
        // 1x USB-C slot profile
        translate([32.0, 26.0, 1.5])
            cube([usb_w, 8.0, usb_h]);
        // 1x M3 screw counterbore
        translate([42.0, 26.0, 0])
            screw_counterbore(8.0 + eps, screw_d, screw_head_d, screw_head_h);
        // 1x Sample Lid Recess for testing lip slide fit
        translate([22.0, 4.0, 8.0 - lid_recess_depth])
            cube([14.0, lip_thickness + 2*lip_clearance_xy, lid_recess_depth + eps]);
    }
}

// ==============================================================================
// ASSEMBLY, EXPLODED & SLICER PRINT LAYOUT
// ==============================================================================

module assembled_preview() {
    // Base half
    base();

    // UDOO KEY PCB seated on 6 standoffs
    if (show_pcb) {
        translate([pcb_x0, pcb_y0, floor_t + standoff_h])
            udoo_key_pcb_mockup();
    }

    // ST7735 Display module seated in lid pocket
    if (show_display) {
        translate([ui_center_x - tft_pcb_w/2, tft_center_y - tft_pcb_h/2, base_h + lid_h - lid_t - 3.2])
            st7735_display_mockup();
    }

    // Add-on Daughterboard PCB mounted on RP2040 pinout
    if (show_daughterboard && extra_right > 0) {
        daughterboard_mockup();
    }

    // 5 Tactile Switches seated in lid cradles
    if (show_buttons) {
        for (p = btn_coords) {
            translate([p[0], p[1], base_h + lid_h - lid_t - 3.9])
                tactile_switch_6x6_mockup();
        }
    }

    // Lid half (mated)
    translate([0, 0, base_h])
        lid();
}

module exploded_preview() {
    // Base at Z=0
    base();

    // PCB elevated
    if (show_pcb) {
        translate([pcb_x0, pcb_y0, floor_t + standoff_h + explode * 0.35])
            udoo_key_pcb_mockup();
    }

    // Add-on Daughterboard PCB elevated
    if (show_daughterboard && extra_right > 0) {
        translate([0, 0, explode * 0.50])
            daughterboard_mockup();
    }

    // Lid elevated
    translate([0, 0, base_h + explode])
        lid();

    // Display elevated further
    if (show_display) {
        translate([ui_center_x - tft_pcb_w/2, tft_center_y - tft_pcb_h/2, base_h + explode + lid_h - lid_t - 3.2 + explode * 0.4])
            st7735_display_mockup();
    }

    // 5 Tactile Switches elevated below the lid
    if (show_buttons) {
        for (p = btn_coords) {
            translate([p[0], p[1], base_h + explode * 0.65])
                tactile_switch_6x6_mockup();
        }
    }
}

module print_layout(spacing = 12.0) {
    // Base flat on buildplate (Z=0)
    base();

    // Lid rotated 180 degrees so top face rests flat on buildplate (Z=0)
    translate([outer_len + spacing, 0, lid_h])
        rotate([0, 180, 0])
            translate([-outer_len, 0, 0])
                lid();
}

// ==============================================================================
// MASTER DISPATCH
// ==============================================================================

if (part_mode == "base") {
    base();
} else if (part_mode == "lid") {
    lid();
} else if (part_mode == "assembled") {
    assembled_preview();
} else if (part_mode == "exploded") {
    exploded_preview();
} else if (part_mode == "wiring") {
    wiring_preview();
} else if (part_mode == "test_coupon") {
    test_coupon();
} else {
    // Default: "both" (Slicer-ready print layout)
    print_layout();
}
