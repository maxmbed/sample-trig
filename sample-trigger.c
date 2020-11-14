#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample_trig.h"
#include "log.h"

enum key_code {
    key_trig_0 = 'q',
    key_trig_1 = 's',
    key_trig_2 = 'd',
    key_trig_3 = 'f',
    key_trig_4 = 'g',
    key_trig_5 = 'h',
    key_trig_exit   = 'x',
};


int main(int argc, char* argv[]) {

    int num_sample_trig = 0;
    sample_trig_t* sample_list[samples_max] = {0};


    LOG_INFO("Start of %s\n", argv[0]);

    if (argv[1] == NULL) {
        LOG_ERROR("Usage: %s <path sample 1> <path sample 2> ...\n", argv[0]);
        return -1;
    }

    if (argc > samples_max) {
        LOG_ERROR("Maximum %d sample allowed\n", samples_max);
        return -1;
    }

    num_sample_trig = argc -1;

    if (sample_trig_init(sample_list, argv, num_sample_trig)) {
        return -1;
    }

    sleep(1);

    int quit = 0;
    char key_trig[2] = {0};

    while (quit == 0) {

        read(0, key_trig, 2);

        if (key_trig[0] == '\n')
            continue; //filter line feed

        LOG_INFO("Key trig: %c\n", key_trig[0]);
        switch (key_trig[0]) {

            case key_trig_0:

                if (sample_trig(sample_list, sample_0)) {
                    continue;
                }
                break;

            case key_trig_1:

                if (sample_trig(sample_list, sample_1)) {
                    continue;
                }
                break;

            case key_trig_2:

                if (sample_trig(sample_list, sample_2)) {
                    continue;
                }
                break;

            case key_trig_3:

                if (sample_trig(sample_list, sample_3)) {
                    continue;
                }
                break;

            case key_trig_4:

                if (sample_trig(sample_list, sample_4)) {
                    continue;
                }
                break;

            case key_trig_5:

                if (sample_trig(sample_list, sample_5)) {
                    continue;
                }
                break;

            case key_trig_exit:

                if (sample_trig_exit(sample_list, num_sample_trig)) {
                    continue;
                }

                quit = 1;
                break;

            default:
                //Otherwise, ignore other input keys
                break;
        }

        usleep(10000);
    }

    sleep(1); // TODO wait thread to exit (workaround)

    LOG_INFO("EOP\n");
    return 0;
}
