
#include <gtk/gtk.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t mutex;
sem_t semaphore;

#define QUANTUM 3

typedef struct {
    int task_id;
    int priority;
    int total_time;
    int memory_usage;
    int elapsed_time;
    gboolean completed;
    GtkProgressBar *progress_bar;
    GtkLabel *status_label;
} Task;

int num_tasks = 0;
Task *tasks = NULL;

int compare_priority(const void *a, const void *b);

void show_validation_dialog(GtkWidget *parent) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Nombre de tâches enregistré : %d",
        num_tasks);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


// Fonction pour créer et initialiser les tâches
void create_tasks(GtkWidget *widget, gpointer data) {
      num_tasks = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));

    tasks = malloc(num_tasks * sizeof(Task));

    for (int i = 0; i < num_tasks; i++) {
        tasks[i].task_id = i + 1;
        tasks[i].priority = rand() % 10 + 1;
        tasks[i].total_time = rand() % 9 + 4;
        tasks[i].memory_usage = rand() % 500 + 100;
        tasks[i].elapsed_time = 0;
        tasks[i].completed = FALSE;
        tasks[i].progress_bar = NULL;
        tasks[i].status_label = NULL;
    }

    // Afficher la boîte de dialogue de validation
    show_validation_dialog(gtk_widget_get_toplevel(GTK_SPIN_BUTTON(data)));

}


// Fonction pour mettre à jour la barre de progression et le statut de manière sécurisée
gboolean update_progress_bar(Task *task) {
    float fraction = (float)task->elapsed_time / (float)task->total_time;
    gtk_progress_bar_set_fraction(task->progress_bar, fraction);

    char percentage[10];
    snprintf(percentage, sizeof(percentage), "%.0f%%", fraction * 100);
    gtk_progress_bar_set_text(task->progress_bar, percentage);
    gtk_progress_bar_set_show_text(task->progress_bar, TRUE);

    const char *status_text;
    if (task->completed) {
        status_text = "Terminé";
    } else if (task->elapsed_time >= task->total_time) {
        status_text = "En pause";
    } else {
        status_text = "En cours";
    }
    gtk_label_set_text(task->status_label, status_text);

    return FALSE; // Exécuter cette fonction une seule fois par appel de g_idle_add
}

// Fonction exécutée par chaque tâche dans un thread séparé
void *task_function(void *arg) {
    Task *task = (Task *)arg;

    while (task->elapsed_time < task->total_time) {
        // Attendre le sémaphore pour limiter le nombre de tâches en cours
        sem_wait(&semaphore);

        pthread_mutex_lock(&mutex);
        task->completed = FALSE;
        g_idle_add((GSourceFunc)update_progress_bar, task); // Mise à jour : En cours
        pthread_mutex_unlock(&mutex);

        int time_to_run = QUANTUM;
        if (task->elapsed_time + time_to_run > task->total_time) {
            time_to_run = task->total_time - task->elapsed_time;
        }

        for (int i = 0; i < time_to_run; i++) {
            pthread_mutex_lock(&mutex);
            task->elapsed_time++;
            g_idle_add((GSourceFunc)update_progress_bar, task); // Mettre à jour la barre de progression
            pthread_mutex_unlock(&mutex);

            g_main_context_iteration(NULL, FALSE); // Force GTK à traiter les mises à jour
            sleep(1); // Simule le travail
        }

        pthread_mutex_lock(&mutex);
        if (task->elapsed_time >= task->total_time) {
            task->completed = TRUE;
            g_idle_add((GSourceFunc)update_progress_bar, task); // Mise à jour : Terminé
        } else {
            g_idle_add((GSourceFunc)update_progress_bar, task); // Mise à jour : En pause
        }
        pthread_mutex_unlock(&mutex);

        sem_post(&semaphore); // Libérer le sémaphore
        sleep(1); // Pause pour simuler la file d'attente des tâches
    }

    return NULL;
}


// Fonction pour afficher l'état initial des tâches
void show_initial_state(GtkWidget *widget, gpointer data) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "État Initial des Tâches");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    GtkWidget *label_id = gtk_label_new("Tâche ID");
    GtkWidget *label_priority = gtk_label_new("Priorité");
    GtkWidget *label_time = gtk_label_new("Temps");
    gtk_grid_attach(GTK_GRID(grid), label_id, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_priority, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_time, 2, 0, 1, 1);

    for (int i = 0; i < num_tasks; i++) {
        char id_str[10], priority_str[10], time_str[10];
        snprintf(id_str, 10, "Tâche %d", tasks[i].task_id);
        snprintf(priority_str, 10, "%d", tasks[i].priority);
        snprintf(time_str, 10, "%d s", tasks[i].total_time);

        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(id_str), 0, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(priority_str), 1, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(time_str), 2, i + 1, 1, 1);
    }

    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_widget_show_all(window);
}



// Fonction pour afficher le rapport de performance des tâches
void show_performance_report(GtkWidget *widget, gpointer data) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Rapport de Performance");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 0);

    qsort(tasks, num_tasks, sizeof(Task), compare_priority);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    GtkWidget *label_id = gtk_label_new("Tâche ID");
    GtkWidget *label_priority = gtk_label_new("Priorité");
    GtkWidget *label_time = gtk_label_new("Temps");
    GtkWidget *label_memory = gtk_label_new("Mémoire (Mo)");
    gtk_grid_attach(GTK_GRID(grid), label_id, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_priority, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_time, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_memory, 3, 0, 1, 1);

    for (int i = 0; i < num_tasks; i++) {
        char id_str[10], priority_str[10], time_str[10], memory_str[10];
        snprintf(id_str, 10, "Tâche %d", tasks[i].task_id);
        snprintf(priority_str, 10, "%d", tasks[i].priority);
        snprintf(time_str, 10, "%d s", tasks[i].total_time);
        snprintf(memory_str, 10, "%d", tasks[i].memory_usage);

        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(id_str), 0, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(priority_str), 1, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(time_str), 2, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(memory_str), 3, i + 1, 1, 1);
    }

    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_widget_show_all(window);
}



// Fonction de gestion de l'exécution des tâches dans l'interface
void execute(GtkButton *button, gpointer user_data) {
    GtkWidget *task_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(task_window), "Exécution des Tâches");
    gtk_container_set_border_width(GTK_CONTAINER(task_window), 20);
    g_signal_connect(task_window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(task_window), vbox);



    for (int i = 0; i < num_tasks; i++) {
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

        char task_name[20];
        snprintf(task_name, sizeof(task_name), "Tâche %d", tasks[i].task_id);
        GtkWidget *task_label = gtk_label_new(task_name);
        gtk_box_pack_start(GTK_BOX(hbox), task_label, FALSE, FALSE, 0);

        tasks[i].progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(tasks[i].progress_bar), TRUE, TRUE, 0);

        tasks[i].status_label = GTK_LABEL(gtk_label_new("En attente"));
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(tasks[i].status_label), FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    }

    gtk_widget_show_all(task_window);

    // Lancer chaque tâche dans un thread séparé
    pthread_t threads[num_tasks];
    for (int i = 0; i < num_tasks; i++) {
        pthread_create(&threads[i], NULL, task_function, &tasks[i]);
    }

    // Attendre la fin de toutes les tâches
    for (int i = 0; i < num_tasks; i++) {
        pthread_join(threads[i], NULL);
    }
}

// Fonction de comparaison pour qsort
int compare_priority(const void *a, const void *b) {
    Task *taskA = (Task *)a;
    Task *taskB = (Task *)b;
    // Comparaison par priorité
    if (taskA->priority != taskB->priority) {
        return taskB->priority - taskA->priority; // Tri décroissant par priorité
    }
    // Si les priorités sont égales, comparer par temps d'exécution (croissant)
    if (taskA->total_time != taskB->total_time) {
        return taskA->total_time - taskB->total_time; // Tri croissant par temps d'exécution
    }
    // Si les temps d'exécution sont également égaux, comparer par utilisation de mémoire (croissant)
    return taskA->memory_usage - taskB->memory_usage; // Tri croissant par utilisation de mémoire
}


// Reste des fonctions identiques : compare_priority, show_initial_state, show_performance_report, etc.

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Initialiser le mutex et le sémaphore
    pthread_mutex_init(&mutex, NULL);
    sem_init(&semaphore, 0, 2); // Limite de deux tâches simultanées

    // Charger le CSS
    GtkCssProvider *cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "progressbar2.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(cssProvider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gestionnaire de Tâches");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *button_create_tasks = gtk_button_new_with_label("Définir le nombre de tâches");
    GtkWidget *spin_button = gtk_spin_button_new_with_range(1, 15, 1);
    g_signal_connect(button_create_tasks, "clicked", G_CALLBACK(create_tasks), spin_button);
    gtk_box_pack_start(GTK_BOX(vbox), button_create_tasks, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), spin_button, FALSE, FALSE, 0);

    GtkWidget *state_button = gtk_button_new_with_label("Afficher l'état initial");
    g_signal_connect(state_button, "clicked", G_CALLBACK(show_initial_state), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), state_button, TRUE, TRUE, 5);

    GtkWidget *start_button = gtk_button_new_with_label("Démarrer l'exécution");
    g_signal_connect(start_button, "clicked", G_CALLBACK(execute), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), start_button, TRUE, TRUE, 5);

    GtkWidget *report_button = gtk_button_new_with_label("Afficher le rapport de performance");
    g_signal_connect(report_button, "clicked", G_CALLBACK(show_performance_report), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), report_button, TRUE, TRUE, 5);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);

    gtk_main();

    // Libérer la mémoire
    free(tasks);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&semaphore);

    return 0;
}
