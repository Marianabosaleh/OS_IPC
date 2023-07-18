#include <stdio.h>
#include <string.h>

int main_stnc(int argc, char *argv[]);
int main_stnc2(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n");
        printf("stnc: stnc -1 [options]\n");
        printf("stnc2: stnc -2 [options]\n");
        return 1;
    }

    if (strcmp(argv[1], "-1") == 0)
    {
        return main_stnc(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "-2") == 0)
    {
        return main_stnc2(argc - 1, argv + 1);
    }
    else
    {
        printf("Invalid command\n");
        return 1;
    }
}
