#include<stdio.h>
#include<stdint.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<pwd.h>
#include<time.h>


int main(int argc, char* argv[])
{
    int file = open(argv[1],O_RDONLY);
    int output_file = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    struct stat buffer;
    fstat(file, &buffer);
    char sbuffer[100];
    
    
    sprintf(sbuffer, "Total file size: %ld \n",buffer.st_size);
    write(output_file,sbuffer,strlen(sbuffer));

    sprintf(sbuffer, "No of hard links: %ld \n", buffer.st_nlink);
    write(output_file,sbuffer,strlen(sbuffer));

    sprintf(sbuffer, "User ID of owner: %d\n", buffer.st_uid);
    write(output_file,sbuffer,strlen(sbuffer));

    sprintf(sbuffer, "Most recent modify time: %ld \n", buffer.st_mtime);
    write(output_file,sbuffer,strlen(sbuffer));

    sprintf(sbuffer, "User access rights: ");
    //concatenating all the possibilities for user access rights
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


    close(file);
    close(output_file);


    return 0;

}