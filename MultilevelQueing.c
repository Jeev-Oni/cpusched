#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define MAX_PROCESSES 20
#define MAX_TIMELINE 200
#define NUM_QUEUES 3


typedef struct {
char name[10];
int arrival_time;
int burst_time;
int remaining_time;
int priority;
int original_priority;
int time_in_queue;
int time_executing;
int completion_time;
int turnaround_time;
int waiting_time;
int response_time;
gboolean has_started;
} Process;


typedef struct {
char process_name[10];
int start_time;
int duration;
int queue_level;
} TimelineEntry;


Process processes[MAX_PROCESSES];
int process_count = 7;
int current_time = 0;
TimelineEntry timeline[MAX_TIMELINE];
int timeline_count = 0;

// Configurable parameters
int aging_threshold = 5;
int decrease_threshold = 3;
int time_quantum[NUM_QUEUES] = {2, 4, 8};

GtkWidget *drawing_area;
GtkWidget *info_label;
GtkWidget *time_label;
GtkWidget *aging_entry;
GtkWidget *decrease_entry;
GtkWidget *tq_entries[NUM_QUEUES];

GtkWidget *process_entries[MAX_PROCESSES][3];

// Initialize default process values
void init_default_processes() {
    const int default_data[][3] = {
        {1, 20, 3},   // P1: AT=1, BT=20, Priority=3
        {3, 10, 2},   // P2: AT=3, BT=10, Priority=2
        {5, 2, 1},    // P3: AT=5, BT=2, Priority=1
        {8, 7, 2},    // P4: AT=8, BT=7, Priority=2
        {11, 15, 3},  // P5: AT=11, BT=15, Priority=3
        {15, 8, 2},   // P6: AT=15, BT=8, Priority=2
        {20, 4, 1}    // P7: AT=20, BT=4, Priority=1
    };
    
    for (int i = 0; i < 7; i++) {
        sprintf(processes[i].name, "P%d", i + 1);
        processes[i].arrival_time = default_data[i][0];
        processes[i].burst_time = default_data[i][1];
        processes[i].remaining_time = default_data[i][1];
        processes[i].priority = default_data[i][2];
        processes[i].original_priority = default_data[i][2];
        processes[i].time_in_queue = 0;
        processes[i].time_executing = 0;
        processes[i].completion_time = 0;
        processes[i].has_started = FALSE;
    }
}

// Color scheme for different queue levels
void get_queue_color(int queue_level, double *r, double *g, double *b) {
    switch (queue_level) {
        case 1: *r = 0.2; *g = 0.8; *b = 0.3; break;  // Green - High priority
        case 2: *r = 0.9; *g = 0.7; *b = 0.2; break;  // Yellow - Medium priority
        case 3: *r = 0.9; *g = 0.4; *b = 0.4; break;  // Red - Low priority
        default: *r = 0.5; *g = 0.5; *b = 0.5; break; // Gray
    }
}

// Get process color based on name
void get_process_color(const char *name, double *r, double *g, double *b) {
    if (strcmp(name, "IDLE") == 0) {
        *r = 0.85; *g = 0.85; *b = 0.85;
        return;
    }
    
    int hash = 0;
    for (int i = 0; name[i] != '\0'; i++) {
        hash += name[i];
    }
    
    switch (hash % 7) {
        case 0: *r = 0.3; *g = 0.5; *b = 0.9; break;
        case 1: *r = 0.9; *g = 0.3; *b = 0.5; break;
        case 2: *r = 0.5; *g = 0.9; *b = 0.3; break;
        case 3: *r = 0.9; *g = 0.6; *b = 0.2; break;
        case 4: *r = 0.6; *g = 0.3; *b = 0.9; break;
        case 5: *r = 0.3; *g = 0.9; *b = 0.9; break;
        case 6: *r = 0.9; *g = 0.5; *b = 0.7; break;
    }
}

// Drawing callback
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    // Draw CPU Timeline (Gantt Chart)
    int timeline_height = 100;
    int timeline_y = 10;
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 10, timeline_y + 15);
    cairo_show_text(cr, "CPU Execution Timeline (Gantt Chart):");
    
    // Timeline background
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_rectangle(cr, 10, timeline_y + 25, width - 20, timeline_height - 30);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    
    // Draw timeline entries
    if (timeline_count > 0) {
        int max_time = current_time > 0 ? current_time : 1;
        double scale = (width - 40.0) / max_time;
        
        for (int i = 0; i < timeline_count; i++) {
            double x = 20 + timeline[i].start_time * scale;
            double w = timeline[i].duration * scale;
            
            double r, g, b;
            get_process_color(timeline[i].process_name, &r, &g, &b);
            
            cairo_set_source_rgb(cr, r, g, b);
            cairo_rectangle(cr, x, timeline_y + 30, w, timeline_height - 50);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
            
            // Draw queue level indicator
            double qr, qg, qb;
            get_queue_color(timeline[i].queue_level, &qr, &qg, &qb);
            cairo_set_source_rgb(cr, qr, qg, qb);
            cairo_rectangle(cr, x, timeline_y + 30 + timeline_height - 50, w, 5);
            cairo_fill(cr);
            
            if (w > 25) {
                cairo_set_source_rgb(cr, 1, 1, 1);
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 10);
                
                cairo_text_extents_t extents;
                cairo_text_extents(cr, timeline[i].process_name, &extents);
                cairo_move_to(cr, x + (w - extents.width) / 2, timeline_y + 30 + (timeline_height - 55) / 2 + 4);
                cairo_show_text(cr, timeline[i].process_name);
            }
            
            // Time markers
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_set_font_size(cr, 8);
            char time_str[10];
            sprintf(time_str, "%d", timeline[i].start_time);
            cairo_move_to(cr, x - 3, timeline_y + timeline_height - 8);
            cairo_show_text(cr, time_str);
        }
        
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_font_size(cr, 8);
        char time_str[10];
        sprintf(time_str, "%d", current_time);
        double final_x = 20 + current_time * scale;
        cairo_move_to(cr, final_x - 3, timeline_y + timeline_height - 8);
        cairo_show_text(cr, time_str);
    }
    
    // Draw Priority Queues
    int queue_section_y = timeline_y + timeline_height + 20;
    int queue_height = 120;
    
    for (int q = 0; q < NUM_QUEUES; q++) {
        int y = queue_section_y + q * (queue_height + 10);
        
        // Queue header
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13);
        char queue_title[100];
        sprintf(queue_title, "Queue %d (Priority %d, TQ=%d)", q + 1, q + 1, time_quantum[q]);
        cairo_move_to(cr, 10, y + 15);
        cairo_show_text(cr, queue_title);
        
        // Queue box with color coding
        double qr, qg, qb;
        get_queue_color(q + 1, &qr, &qg, &qb);
        cairo_set_source_rgba(cr, qr, qg, qb, 0.2);
        cairo_rectangle(cr, 10, y + 20, width - 20, queue_height - 30);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, qr, qg, qb);
        cairo_set_line_width(cr, 3);
        cairo_stroke(cr);
        
        // Draw processes in this queue
        int x_pos = 20;
        for (int i = 0; i < process_count; i++) {
            if (processes[i].priority == (q + 1) && processes[i].remaining_time > 0) {
                // Determine process state color
                double pr, pg, pb;
                if (processes[i].arrival_time > current_time) {
                    pr = 0.9; pg = 0.6; pb = 0.2; // Orange - not arrived
                } else {
                    pr = 0.2; pg = 0.8; pb = 0.3; // Green - ready
                }
                
                int box_width = 70;
                cairo_set_source_rgb(cr, pr, pg, pb);
                cairo_rectangle(cr, x_pos, y + 30, box_width, queue_height - 50);
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 0, 0, 0);
                cairo_set_line_width(cr, 1);
                cairo_stroke(cr);
                
                // Process info
                cairo_set_source_rgb(cr, 0, 0, 0);
                cairo_set_font_size(cr, 11);
                cairo_move_to(cr, x_pos + 5, y + 45);
                cairo_show_text(cr, processes[i].name);
                
                cairo_set_font_size(cr, 9);
                char info[50];
                sprintf(info, "RT:%d", processes[i].remaining_time);
                cairo_move_to(cr, x_pos + 5, y + 58);
                cairo_show_text(cr, info);
                
                sprintf(info, "W:%d E:%d", processes[i].time_in_queue, processes[i].time_executing);
                cairo_move_to(cr, x_pos + 5, y + 70);
                cairo_show_text(cr, info);
                
                x_pos += box_width + 10;
            }
        }
    }
    
    // Legend
    int legend_y = queue_section_y + NUM_QUEUES * (queue_height + 10) + 10;
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, 10, legend_y);
    cairo_show_text(cr, "Legend - Queue Colors:");
    
    for (int i = 0; i < NUM_QUEUES; i++) {
        double qr, qg, qb;
        get_queue_color(i + 1, &qr, &qg, &qb);
        cairo_set_source_rgb(cr, qr, qg, qb);
        cairo_rectangle(cr, 200 + i * 120, legend_y - 12, 15, 15);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        char label[30];
        sprintf(label, "Priority %d", i + 1);
        cairo_move_to(cr, 220 + i * 120, legend_y);
        cairo_show_text(cr, label);
    }
    
    return FALSE;
}

// Step simulation
void step_simulation(GtkWidget *widget, gpointer data) {
    gboolean executed = FALSE;
    
    // Update waiting time for all arrived processes
    for (int i = 0; i < process_count; i++) {
        if (processes[i].arrival_time <= current_time && processes[i].remaining_time > 0) {
            processes[i].time_in_queue++;
            
            // Check for priority decrease (demotion)
            if (processes[i].time_in_queue >= decrease_threshold && processes[i].priority < NUM_QUEUES) {
                processes[i].priority++;
                processes[i].time_in_queue = 0;
                processes[i].time_executing = 0;
                
                char msg[200];
                sprintf(msg, "%s demoted to Priority %d (waited too long)", 
                        processes[i].name, processes[i].priority);
                gtk_label_set_text(GTK_LABEL(info_label), msg);
            }
        }
    }
    
    // Try to execute from highest priority queue first
    for (int priority = 1; priority <= NUM_QUEUES && !executed; priority++) {
        for (int i = 0; i < process_count && !executed; i++) {
            Process *p = &processes[i];
            
            if (p->priority == priority && p->remaining_time > 0 && p->arrival_time <= current_time) {
                int tq = time_quantum[priority - 1];
                int exec_time = (tq < p->remaining_time) ? tq : p->remaining_time;
                
                // Add to timeline
                if (timeline_count < MAX_TIMELINE) {
                    strcpy(timeline[timeline_count].process_name, p->name);
                    timeline[timeline_count].start_time = current_time;
                    timeline[timeline_count].duration = exec_time;
                    timeline[timeline_count].queue_level = priority;
                    timeline_count++;
                }
                
                p->remaining_time -= exec_time;
                p->time_executing += exec_time;
                p->time_in_queue = 0; // Reset waiting time
                current_time += exec_time;
                
                if (!p->has_started) {
                    p->response_time = current_time - p->arrival_time - exec_time;
                    p->has_started = TRUE;
                }
                
                char info[250];
                sprintf(info, "Executing %s (Priority %d) for %d units | RT:%d | Exec:%d", 
                        p->name, p->priority, exec_time, p->remaining_time, p->time_executing);
                gtk_label_set_text(GTK_LABEL(info_label), info);
                
                // Check for aging (promotion)
                if (p->time_executing >= aging_threshold && p->priority > 1 && p->remaining_time > 0) {
                    p->priority--;
                    p->time_executing = 0;
                    p->time_in_queue = 0;
                    
                    char msg[200];
                    sprintf(msg, "%s promoted to Priority %d (aging)", p->name, p->priority);
                    gtk_label_set_text(GTK_LABEL(info_label), msg);
                }
                
                if (p->remaining_time == 0) {
                    p->completion_time = current_time;
                    p->turnaround_time = p->completion_time - p->arrival_time;
                    p->waiting_time = p->turnaround_time - p->burst_time;
                }
                
                executed = TRUE;
            }
        }
    }
    
    if (!executed) {
        if (timeline_count < MAX_TIMELINE) {
            strcpy(timeline[timeline_count].process_name, "IDLE");
            timeline[timeline_count].start_time = current_time;
            timeline[timeline_count].duration = 1;
            timeline[timeline_count].queue_level = 0;
            timeline_count++;
        }
        current_time++;
        gtk_label_set_text(GTK_LABEL(info_label), "CPU IDLE - No process ready");
    }
    
    char time_str[50];
    sprintf(time_str, "Current Time: %d", current_time);
    gtk_label_set_text(GTK_LABEL(time_label), time_str);
    
    gtk_widget_queue_draw(drawing_area);
}

// Reset simulation
void reset_simulation(GtkWidget *widget, gpointer data) {
    current_time = 0;
    timeline_count = 0;

    for (int i = 0; i < process_count; i++) {
        const char *at_text = gtk_entry_get_text(GTK_ENTRY(process_entries[i][0]));
        const char *bt_text = gtk_entry_get_text(GTK_ENTRY(process_entries[i][1]));
        const char *pr_text = gtk_entry_get_text(GTK_ENTRY(process_entries[i][2]));

        if (strlen(at_text) > 0) processes[i].arrival_time = atoi(at_text);
        if (strlen(bt_text) > 0) processes[i].burst_time = atoi(bt_text);
        if (strlen(pr_text) > 0) {
            int pr = atoi(pr_text);
            if (pr >= 1 && pr <= NUM_QUEUES) processes[i].priority = pr;
        }

        processes[i].remaining_time = processes[i].burst_time;
        processes[i].time_in_queue = 0;
        processes[i].time_executing = 0;
        processes[i].has_started = FALSE;
        processes[i].original_priority = processes[i].priority;
    }

    const char *aging_text = gtk_entry_get_text(GTK_ENTRY(aging_entry));
    const char *decrease_text = gtk_entry_get_text(GTK_ENTRY(decrease_entry));
    if (strlen(aging_text) > 0) aging_threshold = atoi(aging_text);
    if (strlen(decrease_text) > 0) decrease_threshold = atoi(decrease_text);

    // Read TQ values
    for (int q = 0; q < NUM_QUEUES; q++) {
        const char *tq_text = gtk_entry_get_text(GTK_ENTRY(tq_entries[q]));
        if (strlen(tq_text) > 0) {
            int tq = atoi(tq_text);
            if (tq > 0) time_quantum[q] = tq;
        }
    }

    gtk_label_set_text(GTK_LABEL(info_label), "Simulation reset. Click 'Step' to begin.");
    gtk_label_set_text(GTK_LABEL(time_label), "Current Time: 0");
    gtk_widget_queue_draw(drawing_area);
}

int main(int argc, char *argv[]) {
    SetDllDirectoryA("dlls");
    gtk_init(&argc, &argv);

    init_default_processes();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MLQ with Aging & Priority Decrease");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 750);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_hbox);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_hbox), left_panel, FALSE, FALSE, 5);

    GtkWidget *input_label = gtk_label_new("Process Configuration");
    PangoFontDescription *font = pango_font_description_from_string("Sans Bold 12");
    gtk_widget_override_font(input_label, font);
    pango_font_description_free(font);
    gtk_box_pack_start(GTK_BOX(left_panel), input_label, FALSE, FALSE, 5);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_box_pack_start(GTK_BOX(left_panel), grid, FALSE, FALSE, 5);

    GtkWidget *headers[4];
    headers[0] = gtk_label_new("Process");
    headers[1] = gtk_label_new("AT");
    headers[2] = gtk_label_new("BT");
    headers[3] = gtk_label_new("Priority");
    for (int i = 0; i < 4; i++) gtk_grid_attach(GTK_GRID(grid), headers[i], i, 0, 1, 1);

    for (int i = 0; i < 7; i++) {
        char label[10];
        sprintf(label, "P%d", i + 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(label), 0, i + 1, 1, 1);
        for (int j = 0; j < 3; j++) {
            process_entries[i][j] = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(process_entries[i][j]), 5);
            char default_val[10];
            if (j == 0) sprintf(default_val, "%d", processes[i].arrival_time);
            else if (j == 1) sprintf(default_val, "%d", processes[i].burst_time);
            else sprintf(default_val, "%d", processes[i].priority);
            gtk_entry_set_text(GTK_ENTRY(process_entries[i][j]), default_val);
            gtk_grid_attach(GTK_GRID(grid), process_entries[i][j], j + 1, i + 1, 1, 1);
        }
    }

    GtkWidget *param_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(left_panel), param_box, FALSE, FALSE, 10);

    GtkWidget *aging_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(param_box), aging_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(aging_box), gtk_label_new("Aging Time:"), FALSE, FALSE, 0);
    aging_entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(aging_entry), 5);
    gtk_entry_set_text(GTK_ENTRY(aging_entry), "5");
    gtk_box_pack_start(GTK_BOX(aging_box), aging_entry, FALSE, FALSE, 0);

    GtkWidget *decrease_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(param_box), decrease_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(decrease_box), gtk_label_new("Decrease Time:"), FALSE, FALSE, 0);
    decrease_entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(decrease_entry), 5);
    gtk_entry_set_text(GTK_ENTRY(decrease_entry), "3");
    gtk_box_pack_start(GTK_BOX(decrease_box), decrease_entry, FALSE, FALSE, 0);

    // TQ controls
    for (int q = 0; q < NUM_QUEUES; q++) {
        GtkWidget *tq_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(param_box), tq_box, FALSE, FALSE, 0);
        char label[30];
        sprintf(label, "TQ for Queue %d:", q + 1);
        gtk_box_pack_start(GTK_BOX(tq_box), gtk_label_new(label), FALSE, FALSE, 0);
        tq_entries[q] = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(tq_entries[q]), 5);
        char default_val[10];
        sprintf(default_val, "%d", time_quantum[q]);
        gtk_entry_set_text(GTK_ENTRY(tq_entries[q]), default_val);
        gtk_box_pack_start(GTK_BOX(tq_box), tq_entries[q], FALSE, FALSE, 0);
    }

    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_hbox), right_panel, TRUE, TRUE, 5);

    GtkWidget *title = gtk_label_new("Multi-Level Queue with Aging & Priority Decrease");
    font = pango_font_description_from_string("Sans Bold 14");
    gtk_widget_override_font(title, font);
    pango_font_description_free(font);
    gtk_box_pack_start(GTK_BOX(right_panel), title, FALSE, FALSE, 5);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 800, 600);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), NULL);
    gtk_box_pack_start(GTK_BOX(right_panel), drawing_area, TRUE, TRUE, 0);

    time_label = gtk_label_new("Current Time: 0");
    gtk_box_pack_start(GTK_BOX(right_panel), time_label, FALSE, FALSE, 0);

    info_label = gtk_label_new("Click 'Step' to start simulation");
    gtk_box_pack_start(GTK_BOX(right_panel), info_label, FALSE, FALSE, 0);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(right_panel), button_box, FALSE, FALSE, 5);

    GtkWidget *step_btn = gtk_button_new_with_label("Step");
    g_signal_connect(step_btn, "clicked", G_CALLBACK(step_simulation), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), step_btn, TRUE, TRUE, 5);

    GtkWidget *reset_btn = gtk_button_new_with_label("Reposition");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(reset_simulation), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), reset_btn, TRUE, TRUE, 5);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
