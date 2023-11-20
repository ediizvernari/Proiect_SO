#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>


#define PATH_MAX 1000

void process_entry(const char *path, const struct dirent *entry, int output_file) {
    char full_path[PATH_MAX];
    struct stat buffer;
    char sbuffer[100];

    // Construiește calea completă pentru intrarea curentă
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    // Obține informații despre intrarea curentă
    if (lstat(full_path, &buffer) == -1) {
        perror("lstat");
        return;
    }

    if (S_ISREG(buffer.st_mode)) {
        // Este un fișier obișnuit
        sprintf(sbuffer, "Nume fisier: %s\n", entry->d_name);
        write(output_file, sbuffer, strlen(sbuffer));

        if (strstr(entry->d_name, ".bmp") != NULL) {
            sprintf(sbuffer, "Lungime fisier: %ld\n", buffer.st_size);
            write(output_file, sbuffer, strlen(sbuffer));

            sprintf(sbuffer, "Inaltime fisier: %ld\n", buffer.st_size);
            write(output_file, sbuffer, strlen(sbuffer));
        }

        sprintf(sbuffer, "No of hard links: %ld\n", buffer.st_nlink);
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "User ID of owner: %d\n", buffer.st_uid);
        write(output_file, sbuffer, strlen(sbuffer));

        // Data modificarii
        struct tm *tm_info;
        tm_info = localtime(&buffer.st_mtime);
        strftime(sbuffer, sizeof(sbuffer), "Most recent modify time: %Y-%m-%d %H:%M:%S\n", tm_info);
        write(output_file, sbuffer, strlen(sbuffer));

        // Drepturi de acces
        sprintf(sbuffer, "User access rights: ");
        strcat(sbuffer, ((buffer.st_mode & S_IRUSR)) ? "R" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IWUSR)) ? "W" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IXUSR)) ? "X" : "-");
        strcat(sbuffer, "\n");
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Group access rights: ");
        strcat(sbuffer, ((buffer.st_mode & S_IRGRP)) ? "R" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IWGRP)) ? "W" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IXGRP)) ? "X" : "-");
        strcat(sbuffer, "\n");
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Other access rights: ");
        strcat(sbuffer, ((buffer.st_mode & S_IROTH)) ? "R" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IWOTH)) ? "W" : "-");
        strcat(sbuffer, ((buffer.st_mode & S_IXOTH)) ? "X" : "-");
        strcat(sbuffer, "\n");
        write(output_file, sbuffer, strlen(sbuffer));

    } else if (S_ISLNK(buffer.st_mode)) {
        // Este o legatură simbolică
        sprintf(sbuffer, "Nume legatura: %s\n", entry->d_name);
        write(output_file, sbuffer, strlen(sbuffer));

        // Dimensiunea legaturii simbolice
        sprintf(sbuffer, "Dimensiune legatura: %ld\n", buffer.st_size);
        write(output_file, sbuffer, strlen(sbuffer));

        // Dimensiunea fisierului target
        struct stat target_buffer;
        if (stat(full_path, &target_buffer) == 0) {
            sprintf(sbuffer, "Dimensiune fisier target: %ld\n", target_buffer.st_size);
            write(output_file, sbuffer, strlen(sbuffer));
        }

        // Drepturi de acces legatura simbolica
        sprintf(sbuffer, "Drepturi de acces user legatura: %s\n", ((buffer.st_mode & S_IRUSR) ? "R" : "-") );
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Drepturi de acces grup legatura: %s\n", ((buffer.st_mode & S_IRGRP) ? "R" : "-"));
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Drepturi de acces altii legatura: %s\n", ((buffer.st_mode & S_IROTH) ? "R" : "-"));
        write(output_file, sbuffer, strlen(sbuffer));

    } else if (S_ISDIR(buffer.st_mode)) {
        // Este un director
        sprintf(sbuffer, "Nume director: %s\n", entry->d_name);
        write(output_file, sbuffer, strlen(sbuffer));

        struct passwd *user_info = getpwuid(buffer.st_uid);
        if (user_info != NULL) {
            sprintf(sbuffer, "Identificatorul utilizatorului: %s\n", user_info->pw_name);
            write(output_file, sbuffer, strlen(sbuffer));
        }

        // Drepturi de acces pentru director
        sprintf(sbuffer, "Drepturi de acces user: %s\n", ((buffer.st_mode & S_IRUSR) ? "R" : "-"));
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Drepturi de acces grup: %s\n", ((buffer.st_mode & S_IRGRP) ? "R" : "-"));
        write(output_file, sbuffer, strlen(sbuffer));

        sprintf(sbuffer, "Drepturi de acces altii: %s\n", ((buffer.st_mode & S_IROTH) ? "R" : "-"));
        write(output_file, sbuffer, strlen(sbuffer));

    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <director_intrare>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    int output_file = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_file == -1) {
        perror("open");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            process_entry(argv[1], entry, output_file);
        }
    }

    close(output_file);
    closedir(dir);

    return 0;
}