#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/wait.h>

#define PATH_LENGTH 1024

#define NUM 64

#define LEN 256

#define FILE_NUM 1024

typedef void myfunction(char *, struct dirent *, struct stat *);

typedef temp{
    FILE_ONLY,
    DIR_ONLY,
    ALL} FlagArgs;

void search(char *path, int level);
int need_show(struct dirent *dirent, int file_size);
void show(char *name, struct dirent *dirent,
          struct stat *st, myfunction *func);
void show_dir(char *name, struct dirent *dirent, struct stat *st);
void show_link(char *name, struct dirent *dirent, struct stat *st);
void show_file(char *name, struct dirent *dirent, struct stat *st);
void exec(char *name);
void exec_all();

int size_flag = 0;
int size_base = -1;

char *string = NULL;

FlagArgs list = ALL;

char *command_each = NULL;

char *command_all = NULL;

char *file_array[FILE_NUM];

int file_num = 0;

int main(int argc, char *argv[])
{
    char *dir_name = ".";
    char *real_path = NULL;

    int ch;

    while ((ch = getopt(argc, argv, "Ss:f:t:e:E:")) > 0)
    {
        switch (ch)
        {
        case 'S':
            size_flag = 1;
            break;
        case 's':
            size_base = atoi(optarg);
            break;
        case 'f':
            string = optarg;
            break;
        case 't':
        {
            if (strcmp(optarg, "f") == 0)
            {
                list = FILE_ONLY;
            }
            else if (strcmp(optarg, "d") == 0)
            {
                list = DIR_ONLY;
            }
            else
            {
                printf("Invalid argument");
                return 1;
            }
            break;
        }
        case 'e':
            command_each = optarg;
            break;
        case 'E':
            command_all = optarg;
            break;
        default:
            break;
        }
    }

    if (optind < argc)
    {
        dir_name = argv[optind];
    }

    real_path = realpath(dir_name, NULL);
    if (!real_path)
    {
        printf("No such dir");
        return 1;
    }
    printf("%s\n", basename(real_path));
    free(real_path);

    search(dir_name, 1);

    exec_all();

    return 0;
}

void search(char *path, int level)
{
    DIR *parentDir;
    struct dirent *dirent;
    char name[PATH_LENGTH];
    int i;
    struct stat st;

    parentDir = opendir(path);
    if (parentDir == NULL)
    {
        printf("Error opening directory");
        exit(-1);
    }

    while ((dirent = readdir(parentDir)) != NULL)
    {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0)
        {
            continue;
        }

        sprintf(name, "%s/%s", path, dirent->d_name);
        lstat(name, &st);

        if (need_show(dirent, st.st_size))
        {
            for (i = 0; i < level; ++i)
            {
                printf("\t");
            }

            if (dirent->d_type == DT_DIR)
            {
                show(name, dirent, &st, show_dir);
            }
            else if (dirent->d_type == DT_LNK)
            {
                show(name, dirent, &st, show_link);
            }
            else
            {
                show(name, dirent, &st, show_file);
            }

            if (dirent->d_type == DT_REG)
            {
                exec(name);
            }

            if (command_all)
            {
                file_array[file_num] = malloc(strlen(name) + 1);
                strcpy(file_array[file_num], name);
                ++file_num;
            }
        }

        if (dirent->d_type == DT_DIR)
        {
            sprintf(name, "%s/%s", path, dirent->d_name);
            search(name, level + 1);
        }
    }
    closedir(parentDir);
}

int need_show(struct dirent *dirent, int file_size)
{
    if ((list == DIR_ONLY && dirent->d_type != DT_DIR) ||
        (list == FILE_ONLY && dirent->d_type != DT_REG))
    {
        return 0;
    }

    if ((size_flag == 1 || size_base > 0) && dirent->d_type == DT_DIR)
    {
        return 0;
    }

    if (string != NULL && strstr(dirent->d_name, string) == NULL)
    {
        return 0;
    }

    if (dirent->d_type != DT_DIR && file_size < size_base)
    {
        return 0;
    }

    return 1;
}

void show_link(char *name, struct dirent *dirent, struct stat *st)
{
    char *real_path = NULL;

    printf("%s", dirent->d_name);
    real_path = realpath(name, NULL);
    if (real_path)
    {
        printf(" (%s)\n", real_path);
        free(real_path);
        real_path = NULL;
    }
    else
    {
        printf("\n");
    }
}

void show(char *name, struct dirent *dirent,
          struct stat *st, myfunction *func)
{
    func(name, dirent, st);
}

void show_dir(char *name, struct dirent *dirent, struct stat *st)
{
    printf("%s\n", dirent->d_name);
}

void show_file(char *name, struct dirent *dirent, struct stat *st)
{

    if (size_flag)
    {
        printf("%s (%lld)\n", dirent->d_name, st->st_size);
    }
    else
    {
        printf("%s\n", dirent->d_name);
    }
}

void exec(char *name)
{
    char cmd[LEN];
    char *cmds[NUM] = {NULL};
    int num = 0;
    char *part;

    if (!command_each)
    {
        return;
    }

    strcpy(cmd, command_each);

    part = strtok(cmd, " ");
    while (part)
    {
        cmds[num++] = part;
        part = strtok(NULL, " ");
    }

    cmds[num] = name;

    if (fork() == 0)
    {
        if (execvp(cmds[0], cmds) < 0)
        {
            printf("execl failed.\n");
            exit(EXIT_SUCCESS);
        }
    }

    wait(NULL);
}

void exec_all()
{
    char cmd[LEN];
    char *cmds[FILE_NUM] = {NULL};
    int num = 0;
    char *part;
    int i;

    if (file_num == 0)
    {
        return;
    }

    strcpy(cmd, command_all);
    part = strtok(cmd, " ");
    while (part)
    {
        cmds[num++] = part;
        part = strtok(NULL, " ");
    }

    for (i = 0; i < file_num; ++i)
    {
        cmds[num++] = file_array[i];
    }

    if (fork() == 0)
    {
        if (execvp(cmds[0], cmds) < 0)
        {
            printf("execl failed.\n");
            exit(EXIT_SUCCESS);
        }
    }

    wait(NULL);

    for (i = 0; i < file_num; ++i)
    {
        free(file_array[i]);
    }
}
