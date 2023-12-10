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
#include <stdint.h>

#define PATH_MAX 4096

int lines;

typedef struct {
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerM;
    int32_t yPixelsPerM;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
} BMPsizestats;

void ErrorHandler(const char *error) {
    perror(error);
    exit(EXIT_FAILURE);
}

void readFormat(int inputFileDescriptor, BMPsizestats *header) {
    char sig[2];
    if (read(inputFileDescriptor, sig, 2) != 2 || sig[0] != 'B' || sig[1] != 'M') {
        ErrorHandler("The file signature is not BM");
    }

    if (read(inputFileDescriptor, header, sizeof(BMPsizestats)) != sizeof(*header)) {
        ErrorHandler("Cannot read the file");
    }
}

void convert_to_grayscale(const char *input_path) {
    int input_file = open(input_path, O_RDWR);
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

    // Move file pointer to the beginning and write the modified pixel data
    lseek(input_file, sizeof(header), SEEK_SET);
    if (write(input_file, original_image, size) != size) {
        perror("write");
        free(original_image);
        close(input_file);
        exit(EXIT_FAILURE);
    }

    free(original_image);
    close(input_file);
}


void sendpipe1(const char *file_path, int *pfd1, int *pfd2) {
    close(pfd2[0]);
    close(pfd2[1]);
    
    close(pfd1[0]); // close read end of pipe1

    if (dup2(pfd1[1], 1) < 0) { // redirect STDOUT to write in pipe1
        perror("Pipe 1 error");
        exit(EXIT_FAILURE);
    }

    execlp("/bin/cat", "/bin/cat", file_path, NULL);

}

void sendpipe2(int* pfd1, int* pfd2, const char* argv[]) {
    close(pfd1[1]); // close write end of pipe1
    
    if (dup2(pfd1[0], 0) < 0) { // redirect STDIN to read in pipe1
        perror("Pipe 1 error");
        exit(EXIT_FAILURE);
    }

    close(pfd2[0]); // close read end of pipe2

    if (dup2(pfd2[1], 1) < 0) { // redirect STDOUT to write in pipe2
        perror("Pipe 2 error");
        exit(EXIT_FAILURE);
    }

    execlp("/bin/bash", "/bin/bash", "./script.sh", argv[3], NULL);

}


int countgoodsentences(const int *p2fd) {
    char buffer[100] = "";
    if (read(p2fd[0], buffer, sizeof(buffer)) < 0) {
        perror("pipe error");
        exit(EXIT_FAILURE);
    }
    int cnt = atoi(buffer);
    //close(p2fd[0]); // Move this line here
    return cnt;
}


void process_entry(const char *input_path, const struct dirent *entry, const char *output_dir, char* argv[], int* sent_count) {
    char input_full_path[PATH_MAX];
    char output_file_path[PATH_MAX];

    BMPsizestats header;

    snprintf(input_full_path, sizeof(input_full_path), "%s/%s", input_path, entry->d_name);

    int status;

    // Create a child process for BMP conversion if it is a BMP file
    if (strstr(entry->d_name, ".bmp") != NULL) {
        pid_t bmp_child_pid = fork();
        if (bmp_child_pid == 0) {
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s_gri.bmp", output_dir, entry->d_name);

            // Child process for BMP conversion
            convert_to_grayscale(input_full_path);
            exit(EXIT_SUCCESS);
            
        } else if (bmp_child_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        waitpid(bmp_child_pid, &status, 0);
        printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", bmp_child_pid, WEXITSTATUS(status));
    }

        snprintf(output_file_path, sizeof(output_file_path), "%s/%s_statistica.txt", output_dir, entry->d_name);

        int output_file = open(output_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_file == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        int input_file;
        if ((input_file = open(input_full_path, O_RDONLY)) < 0) {
            perror("cannot open file");
            exit(-1);
        }

        // Process the entry and write information to the output file
        struct stat buffer;
        char sbuffer[500];

        if (lstat(input_full_path, &buffer) == -1) {
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if (S_ISREG(buffer.st_mode) && strstr(entry->d_name, ".bmp")) {

            pid_t stat_child_pid = fork();
            if (stat_child_pid == 0) {

                readFormat(input_file,&header);

                sprintf(sbuffer, "Nume fisier: %s\n", entry->d_name);
                write(output_file, sbuffer, strlen(sbuffer));

                sprintf(sbuffer, "Lungime fisier: %d\n", header.width);
                write(output_file, sbuffer, strlen(sbuffer));

                sprintf(sbuffer, "Inaltime fisier: %d\n", header.height);
                write(output_file, sbuffer, strlen(sbuffer));

                sprintf(sbuffer, "Dimensiune fisier: %ld\n", buffer.st_size);
                write(output_file, sbuffer, strlen(sbuffer));

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

                exit(EXIT_SUCCESS);
            }
            else if(stat_child_pid < 0)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            waitpid(stat_child_pid, &status, 0);
            printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid, WEXITSTATUS(status));

        }
        else if(S_ISREG(buffer.st_mode))
        {   
             int pfd1[2];
            int pfd2[2];
            if ((pipe(pfd1) < 0) || (pipe(pfd2) < 0)) {
            perror("Error creating pipes");
            exit(EXIT_FAILURE);
            }
            pid_t stat_child_pid = fork();
            if (stat_child_pid == 0) {

                sprintf(sbuffer, "Nume fisier: %s\n", entry->d_name);
                write(output_file, sbuffer, strlen(sbuffer));

                sprintf(sbuffer, "Dimensiune fisier: %ld\n", buffer.st_size);
                write(output_file, sbuffer, strlen(sbuffer));

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

                sendpipe1(input_full_path,pfd1,pfd2);
                
                exit(EXIT_SUCCESS);
            }
            else if(stat_child_pid < 0)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

    pid_t stat_child_pid2 = fork();
        if (stat_child_pid2 == 0) {
            // Child process for counting good sentences

            // Execute sendpipe2 and countgoodsentences
            sendpipe2(pfd1, pfd2, argv);

            exit(EXIT_SUCCESS);
        } else if (stat_child_pid2 < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // Close pipes in the parent process
        close(pfd1[0]);
        close(pfd1[1]);
        close(pfd2[1]);

        int count = countgoodsentences(pfd2);

            *sent_count += count;

        // Wait for child processes to finish
        waitpid(stat_child_pid, &status, 0);
        printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid, WEXITSTATUS(status));

        waitpid(stat_child_pid2, &status, 0);
        printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid2, WEXITSTATUS(status));


        } else if (S_ISLNK(buffer.st_mode)) {

            pid_t stat_child_pid = fork();
            if (stat_child_pid == 0) {

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

                exit(EXIT_SUCCESS);
            }
            else if(stat_child_pid <0)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            waitpid(stat_child_pid, &status, 0);
            printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid, WEXITSTATUS(status));

        } else if (S_ISDIR(buffer.st_mode)) {

            pid_t stat_child_pid = fork();
            if (stat_child_pid == 0) {

                sprintf(sbuffer, "Nume director: %s\n", entry->d_name);
                write(output_file, sbuffer, strlen(sbuffer));

                // User identifier
                struct passwd *user_info = getpwuid(buffer.st_uid);
                if (user_info != NULL) {
                    sprintf(sbuffer, "Identificatorul utilizatorului: %s\n", user_info->pw_name);
                    write(output_file, sbuffer, strlen(sbuffer));
                }

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

                exit(EXIT_SUCCESS);
            }
            else if(stat_child_pid < 0)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            
            waitpid(stat_child_pid, &status, 0);
            printf("S-a incheiat procesul cu pid-ul %d si codul %d\n", stat_child_pid, WEXITSTATUS(status));

        }
        // Close the output file for statistics
        close(output_file);


}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <director_intrare> <director_iesire>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    int sent_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            process_entry(argv[1], entry, argv[2],argv,&sent_count);
        }
    }

   printf("Au fost identificate in total %d propozitii corecte care contin caracterul %s\n", sent_count, argv[3]);


    closedir(dir);

    return 0;
}