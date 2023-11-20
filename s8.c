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
#include <sys/wait.h>

#define PATH_MAX 1000

void convert_to_grayscale(const char *input_path, const char *output_path) {
    int input_file = open(input_path, O_RDONLY);
    if (input_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char header[54];
    if (read(input_file, header, sizeof(header)) != sizeof(header)) {
        perror("read");
        close(input_file);
        exit(EXIT_FAILURE);
    }

    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int size = width * height * 3;

    unsigned char *original_image = (unsigned char*)malloc(size);
    if (original_image == NULL) {
        perror("malloc");
        close(input_file);
        exit(EXIT_FAILURE);
    }

    if (read(input_file, original_image, size) != size) {
        perror("read");
        free(original_image);
        close(input_file);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < size; i += 3) {
        unsigned char blue = original_image[i];
        unsigned char green = original_image[i + 1];
        unsigned char red = original_image[i + 2];


        unsigned char grayscale = (unsigned char)(0.299 * red + 0.587 * green + 0.114 * blue);

        original_image[i] = original_image[i + 1] = original_image[i + 2] = grayscale;
    }

    int output_file = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_file == -1) {
        perror("open");
        free(original_image);
        close(input_file);
        exit(EXIT_FAILURE);
    }

    if (write(output_file, header, sizeof(header)) != sizeof(header)) {
        perror("write");
        free(original_image);
        close(input_file);
        close(output_file);
        exit(EXIT_FAILURE);
    }

    if (write(output_file, original_image, size) != size) {
        perror("write");
        free(original_image);
        close(input_file);
        close(output_file);
        exit(EXIT_FAILURE);
    }

    free(original_image);
    close(input_file);
    close(output_file);
}

void process_entry(const char *input_path, const struct dirent *entry, const char *output_dir) {
    char input_full_path[PATH_MAX];
    char output_file_path[PATH_MAX];

    snprintf(input_full_path, sizeof(input_full_path), "%s/%s", input_path, entry->d_name);

    // Create a child process for BMP conversion if it is a BMP file
    if (strstr(entry->d_name, ".bmp") != NULL) {
        int bmp_child_pid = fork();
        if (bmp_child_pid == 0) {
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s_gri.bmp", output_dir, entry->d_name);

            // Child process for BMP conversion
            convert_to_grayscale(input_full_path, output_file_path);

            exit(EXIT_SUCCESS);
        } else if (bmp_child_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Create a child process for writing statistics
    int stat_child_pid = fork();
    if (stat_child_pid == 0) {
        snprintf(output_file_path, sizeof(output_file_path), "%s/%s_statistica.txt", output_dir, entry->d_name);

        // Child process for writing statistics
        int output_file = open(output_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_file == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // Process the entry and write information to the output file
        struct stat buffer;
        char sbuffer[100];

        if (lstat(input_full_path, &buffer) == -1) {
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if (S_ISREG(buffer.st_mode)) {
            // Regular file
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

            // Modification time
            struct tm *tm_info;
            tm_info = localtime(&buffer.st_mtime);
            strftime(sbuffer, sizeof(sbuffer), "Most recent modify time: %Y-%m-%d %H:%M:%S\n", tm_info);
            write(output_file, sbuffer, strlen(sbuffer));

            // User access rights
            sprintf(sbuffer, "User access rights: ");
            strcat(sbuffer, ((buffer.st_mode & S_IRUSR)) ? "R" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IWUSR)) ? "W" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IXUSR)) ? "X" : "-");
            strcat(sbuffer, "\n");
            write(output_file, sbuffer, strlen(sbuffer));

            // Group access rights
            sprintf(sbuffer, "Group access rights: ");
            strcat(sbuffer, ((buffer.st_mode & S_IRGRP)) ? "R" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IWGRP)) ? "W" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IXGRP)) ? "X" : "-");
            strcat(sbuffer, "\n");
            write(output_file, sbuffer, strlen(sbuffer));

            // Other access rights
            sprintf(sbuffer, "Other access rights: ");
            strcat(sbuffer, ((buffer.st_mode & S_IROTH)) ? "R" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IWOTH)) ? "W" : "-");
            strcat(sbuffer, ((buffer.st_mode & S_IXOTH)) ? "X" : "-");
            strcat(sbuffer, "\n");
            write(output_file, sbuffer, strlen(sbuffer));

        } else if (S_ISLNK(buffer.st_mode)) {
            // Symbolic link pointing to a regular file
            sprintf(sbuffer, "Nume legatura: %s\n", entry->d_name);
            write(output_file, sbuffer, strlen(sbuffer));

            // Size of the symbolic link
            sprintf(sbuffer, "Dimensiune legatura: %ld\n", buffer.st_size);
            write(output_file, sbuffer, strlen(sbuffer));

            // Size of the target file
            struct stat target_buffer;
            if (stat(input_full_path, &target_buffer) == 0) {
                sprintf(sbuffer, "Dimensiune fisier target: %ld\n", target_buffer.st_size);
                write(output_file, sbuffer, strlen(sbuffer));
            }

            // User access rights for the symbolic link
            sprintf(sbuffer, "Drepturi de acces user legatura: %s\n", ((buffer.st_mode & S_IRUSR) ? "R" : "-") );
            write(output_file, sbuffer, strlen(sbuffer));

            sprintf(sbuffer, "Drepturi de acces grup legatura: %s\n", ((buffer.st_mode & S_IRGRP) ? "R" : "-"));
            write(output_file, sbuffer, strlen(sbuffer));

            sprintf(sbuffer, "Drepturi de acces altii legatura: %s\n", ((buffer.st_mode & S_IROTH) ? "R" : "-"));
            write(output_file, sbuffer, strlen(sbuffer));

        } else if (S_ISDIR(buffer.st_mode)) {
            // Directory
            sprintf(sbuffer, "Nume director: %s\n", entry->d_name);
            write(output_file, sbuffer, strlen(sbuffer));

            // User identifier
            struct passwd *user_info = getpwuid(buffer.st_uid);
            if (user_info != NULL) {
                sprintf(sbuffer, "Identificatorul utilizatorului: %s\n", user_info->pw_name);
                write(output_file, sbuffer, strlen(sbuffer));
            }

            // User access rights for the directory
            sprintf(sbuffer, "Drepturi de acces user: %s\n", ((buffer.st_mode & S_IRUSR) ? "R" : "-"));
            write(output_file, sbuffer, strlen(sbuffer));

            sprintf(sbuffer, "Drepturi de acces grup: %s\n", ((buffer.st_mode & S_IRGRP) ? "R" : "-"));
            write(output_file, sbuffer, strlen(sbuffer));

            sprintf(sbuffer, "Drepturi de acces altii: %s\n", ((buffer.st_mode & S_IROTH) ? "R" : "-"));
            write(output_file, sbuffer, strlen(sbuffer));
        }

        // Write a newline to separate entries in the statistics file
        write(output_file, "\n", 1);

        // Close the output file for statistics
        close(output_file);

        exit(EXIT_SUCCESS);
    } else if (stat_child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    int status;

    pid_t bmp_child_pid = fork();
    if (bmp_child_pid == 0) {
        snprintf(output_file_path, sizeof(output_file_path), "%s/%s_gri.bmp", output_dir, entry->d_name);

        // Child process for BMP conversion
        convert_to_grayscale(input_full_path, output_file_path);

        exit(EXIT_SUCCESS);
    } else if (bmp_child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    waitpid(bmp_child_pid, &status, 0);
    printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", bmp_child_pid, WEXITSTATUS(status));

    waitpid(stat_child_pid, &status, 0);
    printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid, WEXITSTATUS(status));
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <director_intrare> <director_iesire>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            process_entry(argv[1], entry, argv[2]);
        }
    }

    closedir(dir);

    return 0;
}